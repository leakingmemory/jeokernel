//
// Created by sigsegv on 06.05.2021.
//

#include <concurrency/raw_spinlock.h>
#include <cstdint>
#include <utility>

raw_spinlock::raw_spinlock() : cacheline() {
    for (int i = 0; i < 16; i++) {
        cacheline[i] = i;
        _before_cacheline[i] = i - 1;
        _after_cacheline[i] = cacheline[i] ^ _before_cacheline[i];
    }
    cacheline[0] = 0;
    cacheline[1] = 0;
}

uint32_t raw_spinlock::create_ticket() noexcept {
    return __atomic_fetch_add(&cacheline[0], 1, __ATOMIC_ACQ_REL);
}

bool raw_spinlock::try_acquire_ticket(uint32_t ticket) noexcept {
    uint32_t expected = ticket;
    return __atomic_compare_exchange_n(&cacheline[0], &expected, ticket + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

void raw_spinlock::release_ticket() noexcept {
    __atomic_fetch_add(&cacheline[1], 1, __ATOMIC_RELEASE);
    asm volatile("dsb ish\nsev\n" ::: "memory");
}

bool raw_spinlock::try_lock() noexcept {
    return try_acquire_ticket(__atomic_load_n(&cacheline[1], __ATOMIC_ACQUIRE));
}

void raw_spinlock::lock() noexcept {
    uint32_t ticket = create_ticket();
    while (ticket != __atomic_load_n(&cacheline[1], __ATOMIC_ACQUIRE)) {
        asm volatile("yield");
    }
}

void raw_spinlock::unlock() noexcept {
    release_ticket();
}
