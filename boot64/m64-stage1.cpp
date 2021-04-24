//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include "textconsole/b8000logger.h"
#include <pagealloc.h>
#include <multiboot.h>
#include <core/malloc.h>

static const MultibootInfoHeader *multiboot_info = nullptr;

const MultibootInfoHeader &get_multiboot2() {
    return *multiboot_info;
}

extern "C" {

    void start_m64(const MultibootInfoHeader *multibootInfoHeaderPtr) {
        multiboot_info = multibootInfoHeaderPtr;
        /*
         * Let's try to alloc a stack
         */
        uint64_t stack = (uint64_t) pagealloc(16384);
        if (stack != 0) {
            stack += 16384 - 16;
            asm("mov %0, %%rax; mov %%rax,%%rsp; jmp secondboot; hlt;" :: "r"(stack) : "%rax");
        }
        b8000 b8000logger{};
        b8000logger << "Error: Unable to install new stack!\n";
        asm("hlt");
    }

    void init_m64() {
        setup_simplest_malloc_impl();

        b8000logger *b8000Logger = new b8000logger();

        *b8000Logger << "Sizeof pointer: ";
        b8000Logger->print_u16((uint16_t) sizeof(&b8000Logger));
        *b8000Logger << "\n";
        set_klogger(b8000Logger);

        get_klogger() << "Kernel logger setup\n";

        while (1) {
            asm("hlt");
        }
    }
}