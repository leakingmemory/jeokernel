//
// Created by sigsegv on 2/1/22.
//

#include <concurrency/mul_semaphore.h>
#include <concurrency/raw_semaphore.h>
#include <core/scheduler.h>
#include "concurrency/critical_section.h"
#include "mutex"

//#define MUL_SEM_DEBUG

class waiting_mul_semaphore {
private:
    raw_semaphore semaphore;
    std::shared_ptr<task_event_handler_ref> timeout_ref;
public:
    waiting_mul_semaphore() : semaphore(-1), timeout_ref() {
    }
    ~waiting_mul_semaphore() {
        timeout_ref = nullptr;
    }

    void timeout(uint64_t millis) {
        timeout_ref = get_scheduler()->set_timeout_millis(millis, [this] () {
            semaphore.release_from_event();
        });
    }

    void release() {
#ifdef MUL_SEM_DEBUG
        get_klogger() << "semaphore release raw\n";
#endif
        semaphore.release();
    }

    void acquire() {
#ifdef MUL_SEM_DEBUG
        get_klogger() << "semaphore acquire raw\n";
#endif
        semaphore.acquire();
#ifdef MUL_SEM_DEBUG
        get_klogger() << "semaphore released raw\n";
#endif
    }
};

mul_semaphore::mul_semaphore() : spinlock(), waiting(), release_count(0) {
}

void mul_semaphore::acquire(uint64_t timeout_millis) {
#ifdef MUL_SEM_DEBUG
    get_klogger() << "mul_semaphore acquire\n";
#endif
    {
        std::lock_guard lock{spinlock};
        if (release_count > 0) {
            --release_count;
            return;
        }
    }
    auto *slot = new waiting_mul_semaphore();
    slot->timeout(timeout_millis);
    {
        bool released{false};
        {
            std::lock_guard lock{spinlock};
            if (release_count == 0) {
                waiting.push_back(slot);
            } else {
                --release_count;
                released = true;
            }
        }
        if (released) {
            delete slot;
            return;
        }
    }
    slot->acquire();
    {
        std::lock_guard lock{spinlock};
        auto iterator = waiting.begin();
        while (iterator != waiting.end()) {
            if (*iterator == slot) {
                waiting.erase(iterator);
                break;
            }
            ++iterator;
        }
    }
    delete slot;
#ifdef MUL_SEM_DEBUG
    get_klogger() << "mul_semaphore returned\n";
#endif
}

void mul_semaphore::release() {
#ifdef MUL_SEM_DEBUG
    get_klogger() << "mul_semaphore release\n";
#endif
    {
        critical_section cli{};
        std::lock_guard lock{spinlock};
        auto iterator = waiting.begin();
        if (iterator != waiting.end()) {
            (*iterator)->release();
            waiting.erase(iterator);
        } else {
            ++release_count;
        }
    }
#ifdef MUL_SEM_DEBUG
    get_klogger() << "mul_semaphore released\n";
#endif
}
