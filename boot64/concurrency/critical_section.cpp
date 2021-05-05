//
// Created by sigsegv on 04.05.2021.
//

#include <concurrency/critical_section.h>

critical_section::critical_section(bool enter) : activated(enter) {
    if (enter) {
        asm("cli");
    }
}

critical_section::~critical_section() {
    if (activated) {
        asm("sti");
        activated = false;
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