//
// Created by sigsegv on 04.05.2021.
//

#include <concurrency/critical_section.h>
#include <cstdint>

critical_section::critical_section(bool enter) : activated(enter), was_activated(false) {
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
    if (activated && !was_activated) {
        asm("sti");
        activated = false;
    } else if (!activated && was_activated) {
        asm("cli");
        activated = true;
    }
}

void critical_section::enter() {
    activated = true;
    asm("cli");
}

void critical_section::leave() {
    asm("sti");
    activated = false;
}