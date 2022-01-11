//
// Created by sigsegv on 30.04.2021.
//

#include <klogger.h>
#include "PageFault.h"

extern "C" {
    uint64_t pagefault_ip = 0;
}

void PageFault::handle() {
    pagefault_ip = interrupt.rip();
    interrupt.print_debug();
    wild_panic("page fault");
}
