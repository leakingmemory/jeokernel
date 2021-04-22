//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include "textconsole/b8000logger.h"
#include <pagealloc.h>
#include <multiboot.h>

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
        b8000 b8000logger{};
        b8000logger << "Hello world!\n";

        asm("hlt");
        while (1) {
        }
    }
}