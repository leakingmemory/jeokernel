//
// Created by sigsegv on 5/13/21.
//

#include <concurrency/mutex.h>
#include <core/scheduler.h>
#include <concurrency/critical_section.h>
#include <pagealloc.h>
#include <thread>

//#define DEBUG_LOCK_UNLOCK

class task_mutex_handler : public task_event_handler {
private:
    uint64_t lockaddr;
    uint64_t ticket_number;
    uint32_t current_task_id;
public:
    task_mutex_handler(uint32_t current_task_id, uint64_t lockaddr, uint64_t ticket_number) :
            task_event_handler(), current_task_id(current_task_id), lockaddr(lockaddr), ticket_number(ticket_number) {
#ifdef DEBUG_LOCK_UNLOCK
        get_klogger() << "Waiting for " << ticket_number << " on " << current_task_id << "\n";
#endif
    }

    ~task_mutex_handler() {
    }

    void event(uint64_t event_id, uint64_t mutex, uint64_t ticket) override {
        task &running_task = get_scheduler()->get_current_task_with_lock();
        if (event_id == TASK_EVENT_CLEAR_WAIT_MUTEX && current_task_id == running_task.get_id()) {
            if (running_task.remove_event_handler(this)) {
                delete this;
            } else {
                get_klogger() << "Current task ID " << current_task_id << " ticket " << ticket_number << "\n";
                wild_panic("Failed to remove event handler on clear wait mutex");
            }
        } else if (event_id == TASK_EVENT_MUTEX && mutex == lockaddr) {
            task &t = get_scheduler()->get_task_with_lock(current_task_id);
            if (ticket >= ticket_number || (/* 32bit overflow case */ticket_number >= 0xF8000000 && ticket < 0x08000000)) {
                /*
                 * There is a potential for handler added and lock acquired before going to
                 * sleep. Hence the handler will be called on our release instead
                 */
                if (ticket == ticket_number) {
                    t.set_blocked(false);
                    t.resource_acq(1);
                }
                if (t.remove_event_handler(this)) {
                    delete this;
                } else {
                    wild_panic("Failed to remove mutex wait handler");
                }
            }
        }
    }
};

namespace std {
    mutex::mutex() : data() {
        for (int i = 0; i < 16; i++) {
            data.cacheline[i] = i;
            data._before_cacheline[i] = i - 1;
            data._after_cacheline[i] = data.cacheline[i] ^ data._before_cacheline[i];
        }
        data.cacheline[0] = 0;
        data.cacheline[1] = 0;
    }

    uint32_t mutex::create_ticket() noexcept {
        uint32_t ticket;
        asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(data.cacheline[0]), "=rm"(ticket) :: "%rax");
        return ticket;
    }

    bool mutex::try_acquire_ticket(uint32_t ticket) noexcept {
        uint64_t t64{ticket};
        asm(""
            "mov %1, %%rax;"
            "mov %%rax, %%rcx;"
            "dec %%rax;"
            "lock cmpxchgl %%ecx, %0;"
            "jnz unsuccessful_hw_try_lock;"
            "inc %%rax;"
            "unsuccessful_hw_try_lock:"
            "mov %%rax, %1"
        : "+m"(data.cacheline[0]), "=rm"(t64) :: "%rax", "%rcx"
        );
        return ((uint32_t) t64) == ticket;
    }

    uint32_t mutex::release_ticket() noexcept {
        uint32_t ticket;
        asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(data.cacheline[1]), "=rm"(ticket) :: "%rax");
        return ticket + 1;
    }

    uint32_t mutex::current_ticket() noexcept {
        uint32_t ticket;
        asm("xor %%rax, %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(data.cacheline[1]), "=rm"(ticket) :: "%rax");
        return ticket;
    }

    bool mutex::try_lock() noexcept {
        return try_acquire_ticket(data.cacheline[1]);
    }

    void mutex::lock() {
        uint32_t ticket = create_ticket();
#ifdef DEBUG_LOCK_UNLOCK
        get_klogger() << "Lock waiting ticket " << ticket << "\n";
#endif
        if (ticket != current_ticket()) {
            auto *scheduler = get_scheduler();
            //get_scheduler()->event(TASK_EVENT_CLEAR_WAIT_MUTEX, 0, 0);
            task_mutex_handler *handler = new task_mutex_handler(scheduler->get_current_task_id(), (uint64_t) this, ticket);
            {
                //scheduler->set_blocked(true);
                scheduler->add_task_event_handler(handler);
                if (ticket != current_ticket()) {
                    /* Have acquired the lock in between */
                    critical_section cli{};
                    scheduler->set_blocked(true);
                    if (ticket == current_ticket())
                        scheduler->set_blocked(false);
                }
            }
            asm("int $0xFE"); // Task switch request
            while (ticket != current_ticket()) {
                asm("pause; int $0xFE"); // Task switch request
            }
            //while (ticket != current_ticket()) {
            //    asm("pause");
            //}
            scheduler->event(TASK_EVENT_CLEAR_WAIT_MUTEX, 0, 0);
        } else {
            get_scheduler()->set_blocked(false, 1);
        }
    }

    void mutex::unlock() noexcept {
        uint32_t next_ticket = release_ticket();
#ifdef DEBUG_LOCK_UNLOCK
        get_klogger() << "Lock next ticket " << next_ticket << "\n";
#endif
        get_scheduler()->event(TASK_EVENT_MUTEX, (uint64_t) this, next_ticket, -1);
    }
}
