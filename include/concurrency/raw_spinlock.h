//
// Created by sigsegv on 06.05.2021.
//

#ifndef JEOKERNEL_RAW_SPINLOCK_H
#define JEOKERNEL_RAW_SPINLOCK_H

#include <cstdint>
#include "critical_section.h"

class raw_spinlock {
private:
    uint32_t _before_cacheline[16];
    uint32_t cacheline[16];
    uint32_t _after_cacheline[16];
public:
    raw_spinlock();

    raw_spinlock(const raw_spinlock &) = delete;
    raw_spinlock(raw_spinlock &&) = delete;
    raw_spinlock &operator =(const raw_spinlock &) = delete;
    raw_spinlock &operator =(raw_spinlock &&) = delete;

private:
    uint32_t create_ticket() noexcept;
    bool try_acquire_ticket(uint32_t ticket) noexcept;
    void release_ticket() noexcept;
public:
    bool try_lock() noexcept;
    void lock() noexcept;
    void unlock() noexcept;
};

#endif //JEOKERNEL_RAW_SPINLOCK_H
