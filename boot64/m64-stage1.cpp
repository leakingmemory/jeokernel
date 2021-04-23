//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include "textconsole/b8000.h"
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

        b8000 b8000logger{};
        b8000logger << "Test malloc:\n";

        char *str = (char *) malloc(64);
        b8000logger << " mallocated\n";

        str[0] = 'A';
        str[1] = 'B';
        str[2] = 'C';
        str[3] = '\n';
        str[4] = 0;
        b8000logger << str;

        free(str);

        b8000logger << " freed\n";

        destroy_simplest_malloc_impl();

        b8000logger << " destroyed\n";

        asm("hlt");
        while (1) {
        }
    }
}