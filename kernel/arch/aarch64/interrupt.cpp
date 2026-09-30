//
// Created by sigsegv on 9/25/26.
//

#include <klogger.h>
#include <interrupt.h>
#include <tuple>
#include <vector>
#include "../../HardwareInterrupts.h"
#include "../../CpuInterrupts.h"

bool Interrupt::has_error_code() const {
    return _has_error_code;
}

void Interrupt::apply_error_code_correction() {
    _has_error_code = true;
}

caller_stack Interrupt::get_caller_stack() const {
    if (_cpu_frame->sp != 0) {
        return get_caller_stack(_cpu_frame->sp);
    }
    return get_caller_stack(_frame->x29);
}

caller_stack Interrupt::get_caller_stack(uint64_t framePointer) const {
    caller_stack stack{};
    stack.pointer = (uint8_t *) (void *) framePointer;

    uint64_t start = (uint64_t) stack.pointer;
    uint64_t end_vpage = start;
    {
        end_vpage &= 0xFFFFFFFFFFFFF000ULL;
        uint64_t start_vpage = end_vpage;
        do {
            std::optional<pageentr> pe = get_pageentr(end_vpage);
            if (!pe || !pe->present() || pe->os_virt_avail()) {
                break;
            }
            if (end_vpage != start_vpage && pe->os_virt_start()) {
                break;
            }
            end_vpage += 4096;
        } while (true);
    }
    if (end_vpage <= start) {
        stack.size = 0;
        return stack;
    }
    stack.size = end_vpage - start;
    return stack;
}

std::vector<std::tuple<size_t, uint64_t>> Interrupt::unwind_stack(const caller_stack &stack, uint64_t fp) const {
    std::vector<std::tuple<size_t, uint64_t>> calls{};
    uint64_t stack_start = (uint64_t) stack.pointer;
    uint64_t stack_end = (uint64_t) stack.end();

    while (fp >= stack_start && (fp + 16) <= stack_end) {
        size_t stack_off = fp - stack_start;
        uint64_t next_fp = stack[stack_off];
        uint64_t lr = stack[stack_off + 8];

        calls.push_back(std::make_tuple<size_t, uint64_t>(stack_off + 8, lr));

        if (next_fp <= fp) {
            break;
        }
        fp = next_fp;
    }
    return calls;
}

void Interrupt::print_debug(bool double_fault, bool stack_dump) const {
    (void) double_fault;
    get_klogger() << "Interrupt " << (uint8_t) _interrupt
                  << " at ELR " << (uint64_t) elr()
                  << " ESR " << (uint64_t) esr()
                  << " FAR " << (uint64_t) far()
                  << " SPSR " << (uint64_t) spsr()
                  << " SP " << (uint64_t) sp() << "\n"
                  << "x0=" << (uint64_t) x0() << " x1=" << (uint64_t) x1() << " x2=" << (uint64_t) x2() << " x3=" << (uint64_t) x3() << "\n"
                  << "x4=" << (uint64_t) x4() << " x5=" << (uint64_t) x5() << " x6=" << (uint64_t) x6() << " x7=" << (uint64_t) x7() << "\n"
                  << "x8=" << (uint64_t) x8() << " x9=" << (uint64_t) x9() << " x10=" << (uint64_t) x10() << " x11=" << (uint64_t) x11() << "\n"
                  << "x12=" << (uint64_t) x12() << " x13=" << (uint64_t) x13() << " x14=" << (uint64_t) x14() << " x15=" << (uint64_t) x15() << "\n"
                  << "x16=" << (uint64_t) x16() << " x17=" << (uint64_t) x17() << " x18=" << (uint64_t) x18() << " x19=" << (uint64_t) x19() << "\n"
                  << "x20=" << (uint64_t) x20() << " x21=" << (uint64_t) x21() << " x22=" << (uint64_t) x22() << " x23=" << (uint64_t) x23() << "\n"
                  << "x24=" << (uint64_t) x24() << " x25=" << (uint64_t) x25() << " x26=" << (uint64_t) x26() << " x27=" << (uint64_t) x27() << "\n"
                  << "x28=" << (uint64_t) x28() << " fp=" << (uint64_t) fp() << " lr=" << (uint64_t) lr() << " sp=" << (uint64_t) sp() << "\n"
                  << "tpidr_el0=" << (uint64_t) tpidr_el0() << " tpidr_el1=" << (uint64_t) tpidr_el1() << "\n";

    if (_fpu != nullptr) {
        get_klogger() << "v0 " << (uint64_t) v0_high() << " " << (uint64_t) v0_low()
                      << " v1 " << (uint64_t) v1_high() << " " << (uint64_t) v1_low() << "\n"
                      << "v2 " << (uint64_t) v2_high() << " " << (uint64_t) v2_low()
                      << " v3 " << (uint64_t) v3_high() << " " << (uint64_t) v3_low() << "\n"
                      << "v4 " << (uint64_t) v4_high() << " " << (uint64_t) v4_low()
                      << " v5 " << (uint64_t) v5_high() << " " << (uint64_t) v5_low() << "\n"
                      << "v6 " << (uint64_t) v6_high() << " " << (uint64_t) v6_low()
                      << " v7 " << (uint64_t) v7_high() << " " << (uint64_t) v7_low()
                      << " fpcr=" << (uint32_t) fpcr()
                      << " fpsr=" << (uint32_t) fpsr() << "\n";
    }

    if (!stack_dump) {
        return;
    }

    get_klogger() << "Stack dump:\n";
    caller_stack stack = get_caller_stack();
    if (stack.size == 0 && fp() != 0) {
        stack = get_caller_stack(fp());
    }

    std::vector<std::tuple<size_t, uint64_t>> calls = unwind_stack(stack, fp());
    uint16_t count = 0;
    for (const std::tuple<size_t, uint64_t> &value : calls) {
        ++count;
        uint64_t lraddr = std::get<1>(value);
        get_klogger() << "  [" << (uint16_t) count << "] " << (uint64_t) lraddr << "\n";
    }
}

extern "C" {
    bool interrupt_handler(uint64_t interrupt_vector, InterruptCpuFrame *cpu_frame, InterruptStackFrame *stack_frame, aarch64_fpu_state *fpu) {
        Interrupt interrupt{cpu_frame, stack_frame, fpu, static_cast<uint8_t>(interrupt_vector)};
        if (interrupt_vector < CPU_INT_N) {
            bool handled = get_cpu_interrupt_handler().handle(interrupt);
            if (handled) {
                return false;
            }
        }
        get_klogger() << "\nReceived exception/interrupt vector " << (uint64_t) interrupt_vector << "\n";
        interrupt.print_debug();
        wild_panic("unhandled aarch64 exception");
        return false;
    }
}
