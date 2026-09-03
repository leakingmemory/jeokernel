//
// Created by sigsegv on 04.05.2021.
//

#include <concurrency/critical_section.h>
#include <cstdint>

bool critical_section::has_interrupts_disabled() {
    uint64_t daif;
    asm volatile("mrs %0, daif" : "=r"(daif));
    return (daif & (1 << 7)) != 0;
}

critical_section::critical_section(bool enter) : entered(enter), activated(enter), was_activated(false) {
    uint64_t daif;
    asm volatile("mrs %0, daif" : "=r"(daif));
    if ((daif & (1 << 7)) != 0) {
        was_activated = true;
    }
    if (enter) {
        asm volatile("msr daifset, #2" ::: "memory");
    } else {
        if ((daif & (1 << 7)) != 0) {
            activated = true;
        }
    }
}

critical_section::~critical_section() {
    if (entered) {
        if (activated && !was_activated) {
            asm volatile("msr daifclr, #2" ::: "memory");
            activated = false;
        } else if (!activated && was_activated) {
            asm volatile("msr daifset, #2" ::: "memory");
            activated = true;
        }
        entered = false;
    }
}

void critical_section::enter() {
    entered = true;
    activated = true;
    asm volatile("msr daifset, #2" ::: "memory");
}

void critical_section::leave() {
    asm volatile("msr daifclr, #2" ::: "memory");
    entered = true;
    activated = false;
}
