//
// Created by sigsegv on 29.04.2021.
//

#ifndef JEOKERNEL_INTERRUPT_FRAME_H
#define JEOKERNEL_INTERRUPT_FRAME_H

#ifndef UNIT_TESTING
#include <pagetable.h>
#endif
#include <vector>
#include <tuple>

struct x86_fpu_state {
    /*   0 */ uint16_t fcw;
    /*   2 */ uint16_t fsw;
    /*   4 */ uint8_t ftw;
    /*   5 */ uint8_t reserved1;
    /*   6 */ uint16_t fop;
    /*   8 */ uint32_t fpu_ip;
    /*  12 */ uint16_t cs;
    /*  14 */ uint16_t reserved2;
    /*  16 */ uint32_t fpu_dp;
    /*  20 */ uint16_t ds;
    /*  22 */ uint16_t reserved3;
    /*  24 */ uint32_t mxcsr;
    /*  28 */ uint32_t mxcsr_mask;
    /*  32 */ uint64_t st0_mm0_low;
    /*  40 */ uint16_t st0_mm0_high;
    /*  42 */ uint32_t reserved4_low;
    /*  46 */ uint16_t reserved4_high;
    /*  48 */ uint64_t st1_mm1_low;
    /*  56 */ uint16_t st1_mm1_high;
    /*  58 */ uint32_t reserved5_low;
    /*  62 */ uint16_t reserved5_high;
    /*  64 */ uint64_t st2_mm2_low;
    /*  72 */ uint16_t st2_mm2_high;
    /*  74 */ uint32_t reserved6_low;
    /*  78 */ uint16_t reserved6_high;
    /*  80 */ uint64_t st3_mm3_low;
    /*  88 */ uint16_t st3_mm3_high;
    /*  90 */ uint32_t reserved7_low;
    /*  94 */ uint16_t reserved7_high;
    /*  96 */ uint64_t st4_mm4_low;
    /* 104 */ uint16_t st4_mm4_high;
    /* 106 */ uint32_t reserved8_low;
    /* 110 */ uint16_t reserved8_high;
    /* 112 */ uint64_t st5_mm5_low;
    /* 120 */ uint16_t st5_mm5_high;
    /* 122 */ uint32_t reserved9_low;
    /* 126 */ uint16_t reserved9_high;
    /* 128 */ uint64_t st6_mm6_low;
    /* 136 */ uint16_t st6_mm6_high;
    /* 138 */ uint32_t reserved10_low;
    /* 142 */ uint16_t reserved10_high;
    /* 144 */ uint64_t st7_mm7_low;
    /* 152 */ uint16_t st7_mm7_high;
    /* 154 */ uint32_t reserved11_low;
    /* 158 */ uint16_t reserved11_high;
    /* 160 */ uint64_t xmm0_low;
    /* 168 */ uint64_t xmm0_high;
    /* 176 */ uint64_t xmm1_low;
    /* 184 */ uint64_t xmm1_high;
    /* 192 */ uint64_t xmm2_low;
    /* 200 */ uint64_t xmm2_high;
    /* 208 */ uint64_t xmm3_low;
    /* 216 */ uint64_t xmm3_high;
    /* 224 */ uint64_t xmm4_low;
    /* 232 */ uint64_t xmm4_high;
    /* 240 */ uint64_t xmm5_low;
    /* 248 */ uint64_t xmm5_high;
    /* 256 */ uint64_t xmm6_low;
    /* 264 */ uint64_t xmm6_high;
    /* 272 */ uint64_t xmm7_low;
    /* 280 */ uint64_t xmm7_high;
    /* 288 */ uint64_t reserved12[28];
} __attribute__((__packed__));
static_assert(sizeof(x86_fpu_state) == 512);

struct InterruptStackFrame {
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
} __attribute__((__packed__));

struct InterruptCpuFrame {
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
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
        return (i+7) < size ? *((uint64_t *) (void *) &(((uint8_t *) pointer)[i])) : 0;
    }
};

class Interrupt {
private:
    InterruptCpuFrame *_cpu_frame;
    InterruptStackFrame *_frame;
    x86_fpu_state *_fpu;
    uint8_t _interrupt;
    bool _has_error_code;
public:
    Interrupt(InterruptCpuFrame *cpu_frame, InterruptStackFrame *frame, x86_fpu_state *fpu, uint8_t interrupt) : _cpu_frame(cpu_frame), _frame(frame), _fpu(fpu), _interrupt(interrupt), _has_error_code(false) {
    }

    void apply_error_code_correction();

    const InterruptCpuFrame &get_cpu_frame() const {
        return *_cpu_frame;
    }
    void set_cpu_frame(InterruptCpuFrame &frame) {
        _cpu_frame->rflags = frame.rflags;
        _cpu_frame->cs = frame.cs;
        _cpu_frame->rip = frame.rip;
        _cpu_frame->rsp = frame.rsp;
        _cpu_frame->ss = frame.ss;
    }
    InterruptStackFrame &get_cpu_state() {
        return *_frame;
    }
    x86_fpu_state &get_fpu_state() {
        return *_fpu;
    }

    uint64_t rax() const {
        return _frame->rax;
    }
    uint64_t rbx() const {
        return _frame->rbx;
    }
    uint64_t rcx() const {
        return _frame->rcx;
    }
    uint64_t rdx() const {
        return _frame->rdx;
    }
    uint64_t rbp() const {
        return _frame->rbp;
    }
    uint64_t rdi() const {
        return _frame->rdi;
    }
    uint64_t rsi() const {
        return _frame->rsi;
    }
    uint64_t r8() const {
        return _frame->r8;
    }
    uint64_t r9() const {
        return _frame->r9;
    }
    uint64_t r10() const {
        return _frame->r10;
    }
    uint64_t r11() const {
        return _frame->r11;
    }
    uint64_t r12() const {
        return _frame->r12;
    }
    uint64_t r13() const {
        return _frame->r13;
    }
    uint64_t r14() const {
        return _frame->r14;
    }
    uint64_t r15() const {
        return _frame->r15;
    }

    uint16_t ds() const {
        return _frame->ds;
    }
    uint16_t es() const {
        return _frame->ds;
    }
    uint16_t fs() const {
        return _frame->ds;
    }
    uint16_t gs() const {
        return _frame->ds;
    }

    bool has_error_code() const;

    uint32_t error_code() const {
        return _cpu_frame->error_code;
    }

    uint16_t cs() const {
        return _cpu_frame->cs;
    }
    uint64_t rip() const {
        return _cpu_frame->rip;
    }
    uint64_t rflags() const {
        return _cpu_frame->rflags;
    }
    uint64_t rsp() const {
        return _cpu_frame->rsp;
    }
    uint16_t ss() const {
        return _cpu_frame->ss;
    }

    uint64_t xmm0_low() const {
        return _fpu->xmm0_low;
    }
    uint64_t xmm0_high() const {
        return _fpu->xmm0_high;
    }

    uint64_t xmm1_low() const {
        return _fpu->xmm1_low;
    }
    uint64_t xmm1_high() const {
        return _fpu->xmm1_high;
    }

    uint64_t xmm2_low() const {
        return _fpu->xmm2_low;
    }
    uint64_t xmm2_high() const {
        return _fpu->xmm2_high;
    }

    uint64_t xmm3_low() const {
        return _fpu->xmm3_low;
    }
    uint64_t xmm3_high() const {
        return _fpu->xmm3_high;
    }

    uint64_t xmm4_low() const {
        return _fpu->xmm4_low;
    }
    uint64_t xmm4_high() const {
        return _fpu->xmm4_high;
    }

    uint64_t xmm5_low() const {
        return _fpu->xmm5_low;
    }
    uint64_t xmm5_high() const {
        return _fpu->xmm5_high;
    }

    uint64_t xmm6_low() const {
        return _fpu->xmm6_low;
    }
    uint64_t xmm6_high() const {
        return _fpu->xmm6_high;
    }

    uint64_t xmm7_low() const {
        return _fpu->xmm7_low;
    }
    uint64_t xmm7_high() const {
        return _fpu->xmm7_high;
    }

    uint8_t interrupt_vector() const {
        return _interrupt;
    }

    caller_stack get_caller_stack() const;
    caller_stack get_caller_stack(uint64_t framePointer) const;

    /**
     *
     * @return 64bit offset in stack and RIP address for each call
     */
    [[nodiscard]] std::vector<std::tuple<size_t,uint64_t>> unwind_stack(const caller_stack &stack, uint64_t rbp) const;

    void print_debug(bool double_fault = false) const;
};

#endif //JEOKERNEL_INTERRUPT_FRAME_H
