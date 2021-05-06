//
// Created by sigsegv on 06.05.2021.
//

#include <concurrency/hw_spinlock.h>
#include <cstdint>

hw_spinlock::hw_spinlock() : cacheline() {
    cacheline[0] = 0;
    cacheline[1] = 0;
}

uint32_t hw_spinlock::create_ticket() noexcept {
    uint32_t ticket;
    asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(cacheline[0]), "=rm"(ticket) :: "%rax");
    return ticket;
}

void hw_spinlock::release_ticket() noexcept {
    asm("lock incl %0" : "+m"(cacheline[1]));
}

bool hw_spinlock::try_lock() noexcept {
    uint32_t ticket = create_ticket();
    if (ticket == cacheline[1]) {
        return true;
    } else {
        release_ticket();
        return false;
    }
}

void hw_spinlock::lock() noexcept {
    uint32_t ticket = create_ticket();
    while (ticket != cacheline[1]) {
        asm("pause");
    }
}

void hw_spinlock::unlock() noexcept {
    release_ticket();
}
