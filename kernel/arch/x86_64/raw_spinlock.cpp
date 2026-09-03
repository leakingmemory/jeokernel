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
    uint32_t ticket;
    asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(cacheline[0]), "=rm"(ticket) :: "%rax");
    return ticket;
}

bool raw_spinlock::try_acquire_ticket(uint32_t ticket) noexcept {
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

void raw_spinlock::release_ticket() noexcept {
    asm("lock incl %0" : "+m"(cacheline[1]));
}

bool raw_spinlock::try_lock() noexcept {
    if (try_acquire_ticket(cacheline[1])) {
        return true;
    }
    return false;
}

void raw_spinlock::lock() noexcept {
    uint32_t ticket = create_ticket();
    while (ticket != cacheline[1]) {
        asm("pause");
    }
}

void raw_spinlock::unlock() noexcept {
    release_ticket();
}
