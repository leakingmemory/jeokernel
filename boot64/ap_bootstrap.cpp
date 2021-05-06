//
// Created by sigsegv on 06.05.2021.
//

#include <klogger.h>
#include <stack.h>

extern "C" {
    void ap_bootstrap() {
        get_klogger() << "In AP\n";
        auto *ap_stack = new normal_stack;
        uint64_t stack = ap_stack->get_addr();
        asm("mov %0, %%rax; mov %%rax,%%rsp; mov %1, %%rdi; jmp ap_with_stack; hlt;" :: "r"(stack), "r"(ap_stack) : "%rax");
    }

    void ap_boot(normal_stack *stack) {
        get_klogger() << "In AP with stack\n";
        while (1) {
            asm("hlt");
        }
    }
}