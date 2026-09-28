//
// Created by sigsegv on 9/25/26.
//

#ifndef JEOKERNEL_INTERRUPT_FRAME_H
#define JEOKERNEL_INTERRUPT_FRAME_H

#ifndef UNIT_TESTING
#include <pagetable.h>
#endif
#include <stdint.h>
#include <vector>
#include <tuple>

struct aarch64_simd_reg {
    uint64_t low;
    uint64_t high;
} __attribute__((__packed__));

struct aarch64_fpu_state {
    aarch64_simd_reg v[32];
    uint32_t fpcr;
    uint32_t fpsr;
    uint64_t reserved;
} __attribute__((__packed__));

using arm64_fpu_state = aarch64_fpu_state;
using x86_fpu_state = aarch64_fpu_state;

struct InterruptStackFrame {
    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t x9;
    uint64_t x10;
    uint64_t x11;
    uint64_t x12;
    uint64_t x13;
    uint64_t x14;
    uint64_t x15;
    uint64_t x16;
    uint64_t x17;
    uint64_t x18;
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29; // Frame Pointer (fp)
    uint64_t x30; // Link Register (lr)
    uint64_t tpidr_el0;
    uint64_t tpidrro_el0;
    uint64_t tpidr_el1;
} __attribute__((__packed__));

struct InterruptCpuFrame {
    uint64_t esr;
    uint64_t far;
    uint64_t elr;
    uint64_t spsr;
    uint64_t sp;
} __attribute__((__packed__));

struct caller_stack {
    void *pointer;
    size_t size;

    void *end() const {
        return (void *) (((uint8_t *) pointer) + size);
    }
    size_t length() const {
        return size;
    }
    uint64_t operator [] (size_t i) const {
        return (i + 7) < size ? *((uint64_t *) (void *) &(((uint8_t *) pointer)[i])) : 0;
    }
};

#include <interrupt.h>

#endif //JEOKERNEL_INTERRUPT_FRAME_H
