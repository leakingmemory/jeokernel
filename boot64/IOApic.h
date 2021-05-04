//
// Created by sigsegv on 02.05.2021.
//

#ifndef JEOKERNEL_IOAPIC_H
#define JEOKERNEL_IOAPIC_H

#include <cstdint>
#include "vmem.h"
#include "cpu_mpfp.h"

class IOApicReg {
private:
    uint32_t *pointer;
    uint8_t reg;
public:
    IOApicReg(uint32_t *pointer, uint8_t reg) : pointer(pointer), reg(reg) {
    }

    IOApicReg &operator = (uint32_t value) {
        pointer[0] = reg;
        pointer[4] = value;
        return *this;
    }

    operator uint32_t () {
        pointer[0] = reg;
        return pointer[4];
    }
};

class IOApic {
private:
    vmem vm;
    uint32_t *pointer;
public:
    explicit IOApic(const cpu_mpfp &mpc);

    IOApicReg operator [] (uint8_t reg) {
        return IOApicReg(pointer, reg);
    }

    uint8_t get_num_vectors() {
        return (((*this)[1] >> 16) & 0xff);
    }

    void set_vector_interrupt(uint8_t vector, uint8_t intr) {
        uint8_t reg = (vector * 2) + 0x10;
        uint32_t val = (*this)[reg];
        uint32_t newval = (val & 0xFFFFFF00) | intr;
        (*this)[reg] = newval;
    }
};


#endif //JEOKERNEL_IOAPIC_H
