//
// Created by sigsegv on 9/25/26.
//

#ifndef JEOKERNEL_INTERRUPT_H
#define JEOKERNEL_INTERRUPT_H

#include <interrupt_frame.h>

class Interrupt {
private:
    InterruptCpuFrame *_cpu_frame;
    InterruptStackFrame *_frame;
    aarch64_fpu_state *_fpu;
    uint8_t _interrupt;
    bool _has_error_code;
public:
    Interrupt(InterruptCpuFrame *cpu_frame, InterruptStackFrame *frame, aarch64_fpu_state *fpu, uint8_t interrupt)
        : _cpu_frame(cpu_frame), _frame(frame), _fpu(fpu), _interrupt(interrupt), _has_error_code(false) {
    }

    void apply_error_code_correction();

    InterruptCpuFrame &get_cpu_frame() const {
        return *_cpu_frame;
    }
    void set_cpu_frame(const InterruptCpuFrame &frame) {
        _cpu_frame->esr = frame.esr;
        _cpu_frame->far = frame.far;
        _cpu_frame->elr = frame.elr;
        _cpu_frame->spsr = frame.spsr;
        _cpu_frame->sp = frame.sp;
    }
    InterruptStackFrame &get_cpu_state() {
        return *_frame;
    }
    aarch64_fpu_state &get_fpu_state() {
        return *_fpu;
    }

    // AArch64 General Purpose Register Getters:
    uint64_t x0() const { return _frame->x0; }
    uint64_t x1() const { return _frame->x1; }
    uint64_t x2() const { return _frame->x2; }
    uint64_t x3() const { return _frame->x3; }
    uint64_t x4() const { return _frame->x4; }
    uint64_t x5() const { return _frame->x5; }
    uint64_t x6() const { return _frame->x6; }
    uint64_t x7() const { return _frame->x7; }
    uint64_t x8() const { return _frame->x8; }
    uint64_t x9() const { return _frame->x9; }
    uint64_t x10() const { return _frame->x10; }
    uint64_t x11() const { return _frame->x11; }
    uint64_t x12() const { return _frame->x12; }
    uint64_t x13() const { return _frame->x13; }
    uint64_t x14() const { return _frame->x14; }
    uint64_t x15() const { return _frame->x15; }
    uint64_t x16() const { return _frame->x16; }
    uint64_t x17() const { return _frame->x17; }
    uint64_t x18() const { return _frame->x18; }
    uint64_t x19() const { return _frame->x19; }
    uint64_t x20() const { return _frame->x20; }
    uint64_t x21() const { return _frame->x21; }
    uint64_t x22() const { return _frame->x22; }
    uint64_t x23() const { return _frame->x23; }
    uint64_t x24() const { return _frame->x24; }
    uint64_t x25() const { return _frame->x25; }
    uint64_t x26() const { return _frame->x26; }
    uint64_t x27() const { return _frame->x27; }
    uint64_t x28() const { return _frame->x28; }
    uint64_t x29() const { return _frame->x29; }
    uint64_t x30() const { return _frame->x30; }

    uint64_t fp() const { return _frame->x29; }
    uint64_t lr() const { return _frame->x30; }

    uint64_t tpidr_el0() const { return _frame->tpidr_el0; }
    uint64_t tpidrro_el0() const { return _frame->tpidrro_el0; }
    uint64_t tpidr_el1() const { return _frame->tpidr_el1; }

    bool has_error_code() const;

    uint32_t error_code() const {
        return static_cast<uint32_t>(_cpu_frame->esr);
    }

    uint64_t esr() const { return _cpu_frame->esr; }
    uint64_t far() const { return _cpu_frame->far; }
    uint64_t elr() const { return _cpu_frame->elr; }
    uint64_t spsr() const { return _cpu_frame->spsr; }
    uint64_t sp() const { return _cpu_frame->sp; }

    // SIMD / FPU getters:
    uint64_t v_low(size_t i) const {
        return (i < 32 && _fpu != nullptr) ? _fpu->v[i].low : 0;
    }
    uint64_t v_high(size_t i) const {
        return (i < 32 && _fpu != nullptr) ? _fpu->v[i].high : 0;
    }

    uint64_t v0_low() const { return v_low(0); }
    uint64_t v0_high() const { return v_high(0); }
    uint64_t v1_low() const { return v_low(1); }
    uint64_t v1_high() const { return v_high(1); }
    uint64_t v2_low() const { return v_low(2); }
    uint64_t v2_high() const { return v_high(2); }
    uint64_t v3_low() const { return v_low(3); }
    uint64_t v3_high() const { return v_high(3); }
    uint64_t v4_low() const { return v_low(4); }
    uint64_t v4_high() const { return v_high(4); }
    uint64_t v5_low() const { return v_low(5); }
    uint64_t v5_high() const { return v_high(5); }
    uint64_t v6_low() const { return v_low(6); }
    uint64_t v6_high() const { return v_high(6); }
    uint64_t v7_low() const { return v_low(7); }
    uint64_t v7_high() const { return v_high(7); }

    uint32_t fpcr() const { return _fpu != nullptr ? _fpu->fpcr : 0; }
    uint32_t fpsr() const { return _fpu != nullptr ? _fpu->fpsr : 0; }

    uint8_t interrupt_vector() const {
        return _interrupt;
    }

    caller_stack get_caller_stack() const;
    caller_stack get_caller_stack(uint64_t framePointer) const;

    /**
     * Unwinds the stack frames using AAPCS64 (frame pointer chain).
     * @return 64-bit offset in stack and LR address for each frame.
     */
    [[nodiscard]] std::vector<std::tuple<size_t, uint64_t>> unwind_stack(const caller_stack &stack, uint64_t fp) const;

    void print_debug(bool double_fault = false, bool stack_dump = true) const;
};

extern "C" {
void install_exception_vectors();
void exception_vector_table();
void vector_curr_el_sp0_sync();
void vector_curr_el_sp0_irq();
void vector_curr_el_sp0_fiq();
void vector_curr_el_sp0_serror();
void vector_curr_el_spx_sync();
void vector_curr_el_spx_irq();
void vector_curr_el_spx_fiq();
void vector_curr_el_spx_serror();
void vector_lower_el_aarch64_sync();
void vector_lower_el_aarch64_irq();
void vector_lower_el_aarch64_fiq();
void vector_lower_el_aarch64_serror();
void vector_lower_el_aarch32_sync();
void vector_lower_el_aarch32_irq();
void vector_lower_el_aarch32_fiq();
void vector_lower_el_aarch32_serror();

bool interrupt_handler(uint64_t interrupt_vector, InterruptCpuFrame *cpu_frame, InterruptStackFrame *stack_frame, aarch64_fpu_state *fpu);
}

#endif //JEOKERNEL_INTERRUPT_H
