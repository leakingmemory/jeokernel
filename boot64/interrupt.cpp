//
// Created by sigsegv on 26.04.2021.
//

#include <klogger.h>
#include <interrupt_frame.h>
#include <string>
#include <core/scheduler.h>
#include "PageFault.h"
#include "CpuExceptionTrap.h"
#include "KernelElf.h"
#include "HardwareInterrupts.h"

void Interrupt::print_debug() const {
    get_klogger() << "Interrupt " << _interrupt << " at " << cs() << ":" << rip() << " rflags " << rflags() << " err " << error_code() << "\n"
    << "ax=" << rax() << " bx="<< rbx() << " cx=" << rcx() << " dx="<< rdx() << "\n"
    << "si=" << rsi() << " di="<< rdi() << " bp=" << rbp() << " sp="<< rsp() << "\n"
    << "r8=" << r8() << " r9="<<r9() << " rA="<<r10()<<" rB=" << r11() << "\n"
    << "rC=" << r12() << " rD="<<r13() <<" rE="<<r14()<<" rF="<< r15() << "\n"
    << "cs=" << cs() << " ds=" << ds() << " es=" << es() << " fs=" << fs() << " gs="
    << gs() << " ss="<< ss() <<"\n";
    get_klogger() << "xmm0 " << xmm0_high() << xmm0_low()
            << " 1 " << xmm1_high() << xmm1_low() << "\n"
            << "xmm2 " << xmm2_high() << xmm2_low()
            << " 3 " << xmm3_high() << xmm3_low() << "\n"
            << "xmm4 " << xmm4_high() << xmm4_low()
            << " 5 " << xmm5_high() << xmm5_low() << "\n"
            << "xmm6 " << xmm6_high() << xmm6_low()
            << " 7 " << xmm7_high() << xmm7_low() << "\n";
    get_klogger() << "Stack:\n";
    const KernelElf &kernelElf = get_kernel_elf();
    {
        std::tuple<uint64_t, const char *> sym = kernelElf.get_symbol((void *) rip());
        uint64_t vaddr = std::get<0>(sym);
        const char *name = std::get<1>(sym);
        uint32_t offset = (uint32_t) (rip() - vaddr);
        get_klogger() << name << "+0x" << offset << "\n";
    }
    /*auto stack = get_caller_stack();
    for (size_t i = 0; i < stack.length(); i++) {
        if ((i & 3) == 0) {
            get_klogger() << "\n";
        }
        get_klogger() << stack[i] << " ";
    }
    get_klogger() << "\n";*/
    std::vector<std::tuple<size_t,uint64_t>> calls = unwind_stack();
    caller_stack stack = get_caller_stack();
    for (const std::tuple<size_t,uint64_t> &value : calls) {
        {
            uint64_t ripaddr = std::get<1>(value);
            std::tuple<uint64_t,const char *> sym = kernelElf.get_symbol((void *) ripaddr);
            uint64_t vaddr = std::get<0>(sym);
            const char * name = std::get<1>(sym);
            uint32_t offset = (uint32_t) (ripaddr - vaddr);
            get_klogger() << name << "+0x" << offset;
        }
        get_klogger() << " (";
        for (size_t par = std::get<0>(value) + 1; par < (3 + std::get<0>(value)) && par < stack.length(); par++) {
            if (par != (std::get<0>(value) + 1)) {
                get_klogger() << ", ";
            }
            get_klogger() << stack[par];
        }
        get_klogger() << ")\n";
    }
}

std::vector<std::tuple<size_t,uint64_t>> Interrupt::unwind_stack() const {
    caller_stack stack = get_caller_stack();
    std::vector<std::tuple<size_t,uint64_t>> calls{};
    uint64_t stack_start = (uint64_t) stack.pointer;
    uint64_t stack_end = (uint64_t) stack.end();
    uint64_t rbp = this->rbp();
    while (rbp > stack_start && rbp < stack_end) {
        size_t stack_off = (rbp - stack_start) / 8;
        rbp = stack[stack_off++];
        calls.push_back(std::make_tuple<size_t,uint64_t>(stack_off, stack[stack_off]));
    }
    return calls;
}

caller_stack Interrupt::get_caller_stack() const {
    caller_stack stack{};
    uint8_t *ptr;
    ptr = (uint8_t *) ((void *) &(_cpu_frame->ss));
    ptr += sizeof(_cpu_frame->ss);
    stack.pointer = ptr;

    uint64_t start = (uint64_t) stack.pointer;
    uint64_t end_vpage = start;
    {
        end_vpage &= 0xFFFFFFFFFFFFF000;
        uint64_t start_vpage = end_vpage;
        do {
            std::optional<pageentr> pe = get_pageentr(end_vpage);
            if (!pe || !pe->present || pe->os_virt_avail) {
                break;
            }
            if (end_vpage != start_vpage && pe->os_virt_start) {
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


bool Interrupt::has_error_code() const {
    return _has_error_code;
}

void Interrupt::apply_error_code_correction() {
    _cpu_frame = (InterruptCpuFrame *) (void *) (((uint8_t *) (void *) _cpu_frame) + 8);
    _has_error_code = true;
}

extern "C" {
    void interrupt_handler(uint64_t interrupt_vector, InterruptStackFrame *stack_frame, x86_fpu_state *fpusse) {
        InterruptCpuFrame *cpuFrame = (InterruptCpuFrame *) (void *) (((uint8_t *) stack_frame) + (sizeof(*stack_frame) - 8)/*error-code-norm-not-avail*/);
        Interrupt interrupt{cpuFrame, stack_frame, fpusse, (uint8_t) interrupt_vector};
        switch (interrupt_vector) {
            case 0: {
                CpuExceptionTrap trap{"division by zero", interrupt};
                trap.handle();
                break;
            }
            case 1: {
                CpuExceptionTrap trap{"debug trap", interrupt};
                trap.handle();
                break;
            }
            case 3: {
                CpuExceptionTrap trap{"breakpoint trap", interrupt};
                trap.handle();
                break;
            }
            case 4: {
                CpuExceptionTrap trap{"overflow", interrupt};
                trap.handle();
                break;
            }
            case 5: {
                CpuExceptionTrap trap{"bound range exceeded", interrupt};
                trap.handle();
                break;
            }
            case 6: {
                CpuExceptionTrap trap{"invalid opcode", interrupt};
                trap.handle();
                break;
            }
            case 7: {
                CpuExceptionTrap trap{"device not available", interrupt};
                trap.handle();
                break;
            }
            case 8: {
                interrupt.apply_error_code_correction();
                CpuExceptionTrap trap{"double fault", interrupt};
                trap.handle();
                break;
            }
            case 0xA: {
                CpuExceptionTrap trap{"invalid tss", interrupt};
                trap.handle();
                break;
            }
            case 0xB: {
                CpuExceptionTrap trap{"segment not present", interrupt};
                trap.handle();
                break;
            }
            case 0xC: {
                CpuExceptionTrap trap{"stack segment fault", interrupt};
                trap.handle();
                break;
            }
            case 0xD: {
                interrupt.apply_error_code_correction();
                CpuExceptionTrap trap{"general protection fault", interrupt};
                trap.handle();
                break;
            }
            case 0xE: {
                interrupt.apply_error_code_correction();
                PageFault trap{interrupt};
                trap.handle();
                break;
            }
            case 0x10: {
                CpuExceptionTrap trap{"x87 floating point exception", interrupt};
                trap.handle();
                break;
            }
            case 0x11: {
                CpuExceptionTrap trap{"alignment check", interrupt};
                trap.handle();
                break;
            }
            case 0x12: {
                CpuExceptionTrap trap{"machine check", interrupt};
                trap.handle();
                break;
            }
            case 0x13: {
                CpuExceptionTrap trap{"simd floating point exception", interrupt};
                trap.handle();
                break;
            }
            case 0x14: {
                CpuExceptionTrap trap{"virtualization exception", interrupt};
                trap.handle();
                break;
            }
            case 0x1E: {
                CpuExceptionTrap trap{"security exception", interrupt};
                trap.handle();
                break;
            }
            case 0xFE: {
                /* Voluntary task switch vector */
                uint8_t cpu{0};
                {
                    cpu_mpfp *mpfp = get_mpfp();
                    LocalApic lapic{*mpfp};
                    cpu = lapic.get_cpu_num(*mpfp);
                }
                get_scheduler()->switch_tasks(interrupt, cpu);
                return;
            }
            case 0xFF: {
                /* send the spurious irqs here */
                return; // ignore
            }
        }
        if (interrupt_vector >= 0x20 && interrupt_vector < (0x20 + HW_INT_N)) {
            bool handled = get_hw_interrupt_handler().handle(interrupt);
            if (!handled) {
                get_klogger() << "\nReceived interrupt " << interrupt_vector << "\n";
                interrupt.print_debug();
                wild_panic("unhandled interrupt");
            }
            return;
        }
        get_klogger() << "\nReceived interrupt " << interrupt_vector << "\n";
        interrupt.print_debug();
        wild_panic("unknown interrupt");
    }
}