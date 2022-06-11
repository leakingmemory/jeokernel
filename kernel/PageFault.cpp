//
// Created by sigsegv on 30.04.2021.
//

#include <klogger.h>
#include <core/scheduler.h>
#include "PageFault.h"

extern "C" {
    uint64_t pagefault_ip = 0;
}

bool PageFault::handle() {
    if ((interrupt.error_code() & 4) != 0) {
        auto *scheduler = get_scheduler();
        if (scheduler->page_fault(interrupt)) {
            return true;
        }
    }
    pagefault_ip = interrupt.rip();
    interrupt.print_debug();
    wild_panic("page fault");
    return false;
}
