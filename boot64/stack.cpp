//
// Created by sigsegv on 25.04.2021.
//

#include <stack.h>
#include <kernelconfig.h>
#include <pagealloc.h>
#include <klogger.h>

normal_stack::normal_stack() : addr(alloc_stack(NORMAL_STACK_SIZE)) {
    if (addr == 0) {
        wild_panic("Stack allocation failure");
    }
}

normal_stack::~normal_stack() {
    if (addr != 0) {
        free_stack(addr);
        addr = 0;
    }
}