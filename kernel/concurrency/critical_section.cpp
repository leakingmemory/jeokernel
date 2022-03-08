//
// Created by sigsegv on 04.05.2021.
//

#include <concurrency/critical_section.h>
#include <cstdint>

bool critical_section::has_interrupts_disabled() {
    uint64_t flags;
    asm("pushfq; pop %%rax; mov %%rax, %0" : "=r"(flags) :: "%rax");
    return (flags & 0x200) == 0;
}

critical_section::critical_section(bool enter) : entered(enter), activated(enter), was_activated(false) {
    if (enter) {
        uint64_t flags;
        asm("pushfq; cli; pop %%rax; mov %%rax, %0" : "=r"(flags) :: "%rax");
        if ((flags & 0x200) == 0) {
            was_activated = true;
        }
    } else {
        uint64_t flags;
        asm("pushfq; pop %%rax; mov %%rax, %0" : "=r"(flags) :: "%rax");
        if ((flags & 0x200) == 0) {
            activated = true;
            was_activated = true;
        }
    }
}

critical_section::~critical_section() {
    if (entered) {
        if (activated && !was_activated) {
            asm("sti");
            activated = false;
        } else if (!activated && was_activated) {
            asm("cli");
            activated = true;
        }
        entered = false;
    }
}

void critical_section::enter() {
    entered = true;
    activated = true;
    asm("cli");
}

void critical_section::leave() {
    asm("sti");
    entered = true;
    activated = false;
}