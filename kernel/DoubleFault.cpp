//
// Created by sigsegv on 12/30/21.
//

#include "DoubleFault.h"

void DoubleFault::handle() {
    interrupt.print_debug(true);
    wild_panic("double fault");
}
