//
// Created by sigsegv on 06.05.2021.
//

#ifndef JEOKERNEL_HW_SPINLOCK_H
#define JEOKERNEL_HW_SPINLOCK_H

#include <cstdint>
#include "critical_section.h"

class hw_spinlock {
private:
    uint32_t _before_cacheline[16];
    uint32_t cacheline[16];
    critical_section_host cli;
    uint32_t _after_cacheline[16 - (sizeof(critical_section_host) / 4)];
public:
    hw_spinlock();

    hw_spinlock(const hw_spinlock &) = delete;
    hw_spinlock(hw_spinlock &&) = delete;
    hw_spinlock &operator =(const hw_spinlock &) = delete;
    hw_spinlock &operator =(hw_spinlock &&) = delete;

private:
    uint32_t create_ticket() noexcept;
    bool try_acquire_ticket(uint32_t ticket) noexcept;
    void release_ticket() noexcept;
public:
    bool try_lock() noexcept;
    void lock() noexcept;
    void unlock() noexcept;
};

#endif //JEOKERNEL_HW_SPINLOCK_H
