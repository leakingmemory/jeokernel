//
// Created by sigsegv on 02.05.2021.
//

#ifndef JEOKERNEL_LOCALAPIC_H
#define JEOKERNEL_LOCALAPIC_H

#include "vmem.h"
#include "cpu_mpfp.h"

#define LAPIC_BASE_MSR 0x1B
#define LAPIC_MSR_ENABLE 0x800

#define LAPIC_TIMER_ONCE    0
#define LAPIC_TIMER_PERIODIC 0x20000

class LocalApic {
private:
    vmem vm;
    uint32_t *pointer;
public:
    explicit LocalApic(const cpu_mpfp &mpc);

    uint64_t get_msr_base_reg();
    void set_msr_base_reg(uint64_t val);

    uint32_t get_lapic_id() const {
        return pointer[0x20 / 4];
    }
    void set_lapic_id(uint32_t lapic_id) {
        pointer[0x20 / 4] = lapic_id;
    }

    int get_cpu_num(const cpu_mpfp &mpc) const {
        uint32_t lapic_id = get_lapic_id() >> 24;
        for (int i = 0; i < mpc.get_num_cpus(); i++) {
            if (mpc.get_cpu(i).local_apic_id == lapic_id) {
                return i;
            }
        }
        return -1;
    }

    uint32_t get_spurious_interrupt_vector() {
        return pointer[0xF0 / 4];
    }
    void set_spurious_interrupt_vector(uint32_t siv) {
        pointer[0xF0 / 4] = siv;
    }

    uint32_t get_timer_reg() {
        return pointer[0x320 / 4];
    }
    void set_timer_reg(uint32_t tv) {
        pointer[0x320 / 4] = tv;
    }

    uint32_t get_lint0_reg() {
        return pointer[0x350 / 4];
    }
    void set_lint0_reg(uint32_t lreg) {
        pointer[0x350 / 4] = lreg;
    }

    uint32_t get_lint1_reg() {
        return pointer[0x360 / 4];
    }
    void set_lint1_reg(uint32_t lreg) {
        pointer[0x360 / 4] = lreg;
    }

    uint32_t get_timer_div() {
        return pointer[0x3E0 / 4];
    }
    void set_timer_div(uint32_t divset) {
        pointer[0x3E0 / 4] = divset;
    }

    void set_timer_count(uint32_t count) {
        pointer[0x380 / 4] = count;
    }

    uint32_t get_timer_value() {
        return pointer[0x390 / 4];
    }

    void enable_apic(bool en = true) {
        uint64_t msr = (get_msr_base_reg() &0x0FFFFFF000) | (en ? LAPIC_MSR_ENABLE : 0);
        get_klogger() << "Set apic msr " << msr << "\n";
        set_msr_base_reg(msr);
        if (en) {
            set_spurious_interrupt_vector(get_spurious_interrupt_vector() | 0x1FF);
        } else {
            set_spurious_interrupt_vector(get_spurious_interrupt_vector() & ~0x100);
        }
    }

    uint32_t set_timer_int_mode(uint8_t intr, uint32_t mode) {
        uint32_t reg = get_timer_reg();
        set_timer_reg(/*(reg & 0xFFFEFF00) |*/ intr | mode);
        return reg;
    }
    void set_lint0_int(uint8_t intr, bool enable = true) {
        uint32_t reg = get_lint0_reg() & 0xFFFEFF00;
        set_lint0_reg(reg | intr | (enable ? 0 : 0x10000));
    }
    void set_lint1_int(uint8_t intr, bool enable = true) {
        uint32_t reg = get_lint1_reg() & 0xFFFEFF00;
        set_lint1_reg(reg | intr | (enable ? 0 : 0x10000));
    }

    void eio() {
        pointer[0xb0/4] = 0;
    }

    void send_init_ipi(uint8_t lapic_id) {
        pointer[0x280/4] = 0;
        pointer[0x310/4] = (pointer[0x310/4] & 0x00CDFFFF) | (((uint32_t) lapic_id) << 24);
        pointer[0x300/4] = (pointer[0x300/4] & 0xFFF00000) |0x004500;
    }
    void send_sipi(uint8_t lapic_id, uint16_t addr) {
        pointer[0x280/4] = 0;
        pointer[0x310/4] = (pointer[0x310/4] & 0x00CDFFFF) | (((uint32_t) lapic_id) << 24);
        pointer[0x300/4] = (pointer[0x300/4] & 0xFFF00000) |0x4600 | ((uint32_t) addr);
    }
};


#endif //JEOKERNEL_LOCALAPIC_H
