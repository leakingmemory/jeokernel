//
// Created by sigsegv on 30.04.2021.
//

#include <klogger.h>
#include "PageFault.h"

void PageFault::handle() {
    interrupt.print_debug();
    wild_panic("page fault");
}
