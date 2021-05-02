//
// Created by sigsegv on 29.04.2021.
//

#ifndef JEOKERNEL_INTERRUPT_FRAME_H
#define JEOKERNEL_INTERRUPT_FRAME_H

#include <pagetable.h>
#include <vector>
#include <tuple>

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

    uint32_t alignment_padding;
    uint32_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((__packed__));

struct caller_stack {
    void *pointer;
    size_t size;

    void *end() {
        return (void *) (((uint8_t *) pointer) + size);
    }
    size_t length() {
        return size / 8;
    }
    uint64_t operator [] (size_t i) {
        return ((uint64_t *) pointer)[i];
    }
};

class Interrupt {
private:
    InterruptStackFrame *_frame;
    uint8_t _interrupt;
public:
    Interrupt(InterruptStackFrame *frame, uint8_t interrupt) : _frame(frame), _interrupt(interrupt) {
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
        return _frame->error_code;
    }

    uint16_t cs() const {
        return _frame->cs;
    }
    uint64_t rip() const {
        return _frame->rip;
    }
    uint64_t rflags() const {
        return _frame->rflags;
    }
    uint64_t rsp() const {
        return _frame->rsp;
    }
    uint16_t ss() const {
        return _frame->ss;
    }

    caller_stack get_caller_stack() const;

    /**
     *
     * @return 64bit offset in stack and RIP address for each call
     */
    [[nodiscard]] std::vector<std::tuple<size_t,uint64_t>> unwind_stack() const;

    void print_debug() const;
};

#endif //JEOKERNEL_INTERRUPT_FRAME_H
