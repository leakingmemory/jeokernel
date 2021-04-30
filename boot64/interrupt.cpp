//
// Created by sigsegv on 26.04.2021.
//

#include <klogger.h>
#include <interrupt_frame.h>
#include <string>
#include "PageFault.h"
#include "CpuExceptionTrap.h"
#include "KernelElf.h"

void Interrupt::print_debug() const {
    get_klogger() << "Interrupt " << _interrupt << " at " << cs() << ":" << rip() << " rflags " << rflags() << "\n"
    << "rax=" << rax() << " rbx="<< rbx() << " rcx=" << rcx() << " rdx="<< rdx() << "\n"
    << "rsi=" << rsi() << " rdi="<< rdi() << " rbp=" << rbp() << " rsp="<< rsp() << "\n"
    << "  r8=" << r8() << "  r9="<<r9() << " r10="<<r10()<<" r11=" << r11() << "\n"
    << " r12=" << r12() << " r13="<<r13() <<" r14="<<r14()<<" r15="<< r15() << "\n"
    << "Stack:\n";
    const KernelElf &kernelElf = get_kernel_elf();
    {
        std::tuple<uint64_t, std::string> sym = kernelElf.get_symbol((void *) rip());
        uint64_t vaddr = std::get<0>(sym);
        std::string name = std::get<1>(sym);
        uint32_t offset = (uint32_t) (rip() - vaddr);
        get_klogger() << name.c_str() << "+0x" << offset << "\n";
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
            std::tuple<uint64_t,std::string> sym = kernelElf.get_symbol((void *) ripaddr);
            uint64_t vaddr = std::get<0>(sym);
            std::string name = std::get<1>(sym);
            uint32_t offset = (uint32_t) (ripaddr - vaddr);
            get_klogger() << name.c_str() << "+0x" << offset;
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
    if (has_error_code()) {
        ptr = (uint8_t *) ((void *) &(_frame->e_ss));
    } else {
        ptr = (uint8_t *) ((void *) &(_frame->ss));
    }
    ptr += sizeof(_frame->ss);
    stack.pointer = ptr;

    uint64_t start = (uint64_t) stack.pointer;
    uint64_t end_vpage = start;
    {
        end_vpage &= 0xFFFFFFFFFFFFF000;
        uint64_t start_vpage = end_vpage;
        do {
            pageentr *pe = get_pageentr64(get_pml4t(), end_vpage);
            if (pe == nullptr || !pe->present || pe->os_virt_avail) {
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
    switch (_interrupt) {
        case 8:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 0x11:
        case 0x1E:
            return true;
        default:
            return false;
    }
}

extern "C" {
    void interrupt_handler(uint64_t interrupt_vector, InterruptStackFrame *stack_frame) {
        Interrupt interrupt{stack_frame, (uint8_t) interrupt_vector};
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
                CpuExceptionTrap trap{"general protection fault", interrupt};
                trap.handle();
                break;
            }
            case 0xE: {
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
        }
        get_klogger() << "\nReceived interrupt " << interrupt_vector << "\n";
        interrupt.print_debug();
        wild_panic("unknown interrupt");
    }
}