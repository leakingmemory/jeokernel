//
// Created by sigsegv on 5/15/21.
//

#include <concurrency/raw_semaphore.h>
#include <core/scheduler.h>
#include <concurrency/critical_section.h>

class task_semaphore_handler : public task_event_handler {
private:
    uint64_t lockaddr;
    uint64_t ticket_number;
    uint32_t current_task_id;
public:
    task_semaphore_handler(uint32_t current_task_id, uint64_t lockaddr, uint64_t ticket_number) :
            task_event_handler(), current_task_id(current_task_id), lockaddr(lockaddr), ticket_number(ticket_number) {
#ifdef DEBUG_LOCK_UNLOCK
        get_klogger() << "Waiting for " << ticket_number << " on " << current_task_id << "\n";
#endif
    }

    ~task_semaphore_handler() {
    }

    void event(uint64_t event_id, uint64_t mutex, uint64_t ticket) override {
        task &running_task = get_scheduler()->get_current_task_with_lock();
        if (event_id == TASK_EVENT_CLEAR_WAIT_SEMAPHORE && current_task_id == running_task.get_id()) {
            if (running_task.remove_event_handler(this)) {
                delete this;
            } else {
                get_klogger() << "Current task ID " << current_task_id << " ticket " << ticket_number << "\n";
                wild_panic("Failed to remove event handler on clear wait semaphore");
            }
        } else if (event_id == TASK_EVENT_SEMAPHORE && mutex == lockaddr) {
            task &t = get_scheduler()->get_task_with_lock(current_task_id);
            if (ticket == ticket_number) {
                t.set_blocked(false);
                if (t.remove_event_handler(this)) {
                    delete this;
                } else {
                    wild_panic("Failed to remove semaphore wait handler");
                }
            }
        }
    }
};

raw_semaphore::raw_semaphore(int32_t count) : _before_cacheline(), cacheline(), _after_cacheline() {
    for (int i = 0; i < 16; i++) {
        cacheline[i] = i;
        _before_cacheline[i] = i - 1;
        _after_cacheline[i] = cacheline[i] ^ _before_cacheline[i];
    }
    cacheline[0] = 0;
    cacheline[1] = (uint32_t) count;
}

uint32_t raw_semaphore::create_ticket() noexcept {
    uint32_t ticket;
    asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(cacheline[0]), "=rm"(ticket) :: "%rax");
    return ticket;
}

bool raw_semaphore::try_acquire_ticket(uint32_t ticket) noexcept {
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
    : "+m"(cacheline[0]), "=rm"(t64) :: "%rax", "%rcx"
    );
    return ((uint32_t) t64) == ticket;
}

uint32_t raw_semaphore::release_ticket() noexcept {
    uint32_t ticket;
    asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(cacheline[1]), "=rm"(ticket) :: "%rax");
    return ticket + 1;
}

uint32_t raw_semaphore::current_ticket() noexcept {
    uint32_t ticket;
    asm("xor %%rax, %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(cacheline[0]), "=rm"(ticket) :: "%rax");
    return ticket;
}

uint32_t raw_semaphore::current_limit() noexcept {
    uint32_t ticket;
    asm("xor %%rax, %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(cacheline[1]), "=rm"(ticket) :: "%rax");
    return ticket;
}

bool raw_semaphore::try_acquire() {
retry_try_raw_semaphore:
    uint32_t ticket_lim = current_limit();
    uint32_t current_tic = current_ticket();
    uint32_t diff = ticket_lim - current_tic;
    if (diff == 0 || diff >= 0x80000000) {
        return false;
    }
    if (!try_acquire_ticket(current_tic + 1)) {
        goto retry_try_raw_semaphore;
    }
    return true;
}

void raw_semaphore::release() {
    uint32_t released = release_ticket();
    get_scheduler()->event(TASK_EVENT_SEMAPHORE, (uint64_t) this, released);
}

void raw_semaphore::acquire() {
    uint32_t current_tic = create_ticket();
    uint32_t ticket_lim = current_limit();
    uint32_t diff = ticket_lim - current_tic;
    if (diff >= 0x80000000) {
        auto *scheduler = get_scheduler();
        scheduler->add_task_event_handler(new task_semaphore_handler(scheduler->get_current_task_id(), (uint64_t) this, current_tic));
        ticket_lim = current_limit();
        diff = ticket_lim - current_tic;
        if (diff >= 0x80000000) {
            critical_section cli{};
            scheduler->set_blocked(true);
            ticket_lim = current_limit();
            diff = ticket_lim - current_tic;
            if (diff < 0x80000000) {
                scheduler->set_blocked(false);
            }
        }
        if (diff >= 0x80000000) {
            asm("int $0xFE"); // Task switch request
        }
        while (true) {
            ticket_lim = current_limit();
            diff = ticket_lim - current_tic;
            if (diff < 0x80000000) {
                scheduler->event(TASK_EVENT_CLEAR_WAIT_SEMAPHORE, (uint64_t) this, current_tic);
                return;
            }
            asm("pause");
        }
    }
}
