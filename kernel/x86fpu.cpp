//
// Created by sigsegv on 3/2/23.
//

#include <core/x86fpu.h>
#include <cstring>

static x86_fpu_state fpu0_frame;

void init_fpu0_initial() {
    uint8_t fpuframebuf[512+16];
    auto addr = (uintptr_t) &(fpuframebuf);
    addr = (addr + 0xF) & ~0xF;
    asm("fxsave (%0)"::"r"(addr));
    memcpy(&fpu0_frame, (void *) addr, 512);
}

const x86_fpu_state &get_fpu0_initial() {
    return fpu0_frame;
}
