//
// Created by sigsegv on 5/13/21.
//

#ifndef JEOKERNEL_CONCURRENCY_MUTEX_H
#define JEOKERNEL_CONCURRENCY_MUTEX_H

#include <cstdint>

namespace std {
    class mutex {
    private:
        uint32_t _before_cacheline[16];
        uint32_t cacheline[16];
        uint32_t _after_cacheline[16];
    public:
        mutex();

        mutex(const mutex &) = delete;
        mutex(mutex &&) = delete;
        mutex &operator =(const mutex &) = delete;
        mutex &operator =(mutex &&) = delete;

    private:
        uint32_t create_ticket() noexcept;
        bool try_acquire_ticket(uint32_t ticket) noexcept;
        uint32_t release_ticket() noexcept;
        uint32_t current_ticket() noexcept;
    public:
        bool try_lock() noexcept;
        void lock();
        void unlock() noexcept;
    };
}

#endif //JEOKERNEL_CONCURRENCY_MUTEX_H
