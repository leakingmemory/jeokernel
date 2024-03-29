//
// Created by sigsegv on 6/15/22.
//

#include <klogger.h>
#include "SyscallSupport.h"
#include "PerCpuVMem.h"

static SyscallSupport *syscallSupport = nullptr;

void *syscall_stack = nullptr;

SyscallSupport::SyscallSupport() {
}

SyscallSupport &SyscallSupport::Instance() {
    if (syscallSupport == nullptr) {
        syscallSupport = new SyscallSupport;
    }
    return *syscallSupport;
}

extern "C" {
void syscall_enter();
}

SyscallSupport &SyscallSupport::CpuSetup() {
    // STAR - Segments
    asm("movl $0xC0000081, %%ecx; rdmsr; movl $0x180008, %%edx; wrmsr; " ::: "%eax", "%ecx", "%edx");
    // LSTAR - Syscall entrypoint 64bit
    asm("movl $0xC0000082, %%ecx; movq %0, %%rdx; movq %%rdx, %%rax; shrq $32, %%rdx; wrmsr;" :: "r"(syscall_enter) : "%ecx", "%rax", "%rdx");
    // CSTAR - Flags to clear - IF/InterruptEnable
    asm("movl $0xC0000084, %%ecx; rdmsr; movq $0x0200, %%rax; wrmsr;" ::: "%ecx", "%rax", "%rdx");
    // EFER - Syscall (SCE) enable
    asm("movl $0xC0000080, %%ecx; rdmsr; orl $0x001, %%eax; wrmsr; " ::: "%eax", "%ecx", "%edx");

    // MSR_GS_BASE -
    asm("movl $0xC0000102, %%ecx; mov %0, %%rax; mov %%rax, %%rdx; shrq $32, %%rdx; wrmsr; " :: "r"(syscall_stack) : "%eax", "%rcx", "%rdx");

    *(((uint64_t *) syscall_stack) - 1) = (uint64_t) syscall_stack;
    return *this;
}

SyscallSupport &SyscallSupport::GlobalSetup() {
    syscall_stack = (void *) PerCpuVMem::Instance().AllocStack();
    if (syscall_stack == nullptr) {
        wild_panic("Unable to allocate syscall stack");
    }
    return *this;
}