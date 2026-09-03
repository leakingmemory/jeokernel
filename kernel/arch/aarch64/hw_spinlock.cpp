//
// Created by sigsegv on 06.05.2021.
//

#include <concurrency/hw_spinlock.h>
#include <cstdint>
#include <utility>

hw_spinlock::hw_spinlock() : cacheline(), cli() {
    for (int i = 0; i < 16; i++) {
        cacheline[i] = i;
        _before_cacheline[i] = i - 1;
        _after_cacheline[i] = cacheline[i] ^ _before_cacheline[i];
    }
    cacheline[0] = 0;
    cacheline[1] = 0;
}

uint32_t hw_spinlock::create_ticket() noexcept {
    return __atomic_fetch_add(&cacheline[0], 1, __ATOMIC_ACQ_REL);
}

bool hw_spinlock::try_acquire_ticket(uint32_t ticket) noexcept {
    uint32_t expected = ticket;
    return __atomic_compare_exchange_n(&cacheline[0], &expected, ticket + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

void hw_spinlock::release_ticket() noexcept {
    __atomic_fetch_add(&cacheline[1], 1, __ATOMIC_RELEASE);
    asm volatile("dsb ish\nsev\n" ::: "memory");
}

bool hw_spinlock::try_lock() noexcept {
    critical_section cli{};
    if (try_acquire_ticket(__atomic_load_n(&cacheline[1], __ATOMIC_ACQUIRE))) {
        this->cli = std::move(cli);
        return true;
    }
    return false;
}

void hw_spinlock::lock() noexcept {
    critical_section cli{};
    uint32_t ticket = create_ticket();
    while (ticket != __atomic_load_n(&cacheline[1], __ATOMIC_ACQUIRE)) {
        asm volatile("yield");
    }
    this->cli = std::move(cli);
}

void hw_spinlock::unlock() noexcept {
    critical_section cli{false};
    this->cli.extract(cli);
    release_ticket();
}
