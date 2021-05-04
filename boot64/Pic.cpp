//
// Created by sigsegv on 02.05.2021.
//

#include "Pic.h"
#include <cpuio.h>
#include <klogger.h>

static volatile uint64_t tsc() {
    uint32_t low;
    uint32_t high;
    asm("rdtsc; mov %%eax, %0; mov %%edx, %1" : "=r"(low), "=r"(high) :: "%rax", "%rdx");
    uint64_t tsc = high;
    tsc = tsc << 32;
    tsc += low;
    return tsc;
}

static void spin_delay(uint64_t cpucycles) {
    uint64_t tsc_target = tsc() + cpucycles;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"
    while (tsc() < tsc_target) {
        asm("nop");
    }
#pragma clang diagnostic pop
}

#define PIC2_CMD 0xa0
#define PIC2_DAT 0xa1
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21

void Pic::disable() {
    // Disable the PIC
    outportb(PIC1_DAT, 0xff);
    outportb(PIC2_DAT, 0xff);
    spin_delay(1000000000);
    // IOMCR send ints through the APICs
    outportb(0x22, 0x70);
    outportb(0x23, 0x1);
    spin_delay(1000000000);
    get_klogger() << "PIC disabled\n";
}

void Pic::remap(uint8_t vec_base) {
    outportb(PIC1_CMD, 0x11); // init
    spin_delay(1000);
    outportb(PIC2_CMD, 0x11); // init
    spin_delay(1000);
    outportb(PIC1_DAT, vec_base); // int base 1
    spin_delay(1000);
    outportb(PIC2_DAT, vec_base + 3); // int base 2
    spin_delay(1000);
    outportb(PIC1_DAT, 4); // config
    spin_delay(1000);
    outportb(PIC2_DAT, 2); // config
    spin_delay(1000);

    outportb(PIC1_DAT, 1); // mode
    spin_delay(1000);
    outportb(PIC2_DAT, 1); // mode
    spin_delay(1000);
}
