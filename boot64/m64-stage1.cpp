//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include <klogger.h>
#include "textconsole/b8000logger.h"
#include <pagealloc.h>

extern "C" {

    void start_m64() {
        /*
         * Let's try to alloc a stack
         */
        uint64_t stack = (uint64_t) pagealloc(16384);
        if (stack != 0) {
            asm("hlt");
        }
    }

    void init_m64() {
        b8000 b8000logger{};
        b8000logger << "Hello world!\n";

        asm("hlt");
        while (1) {
        }
    }
}