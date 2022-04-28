//
// Created by sigsegv on 4/28/22.
//

#include <sched.h>

extern "C" {
    int sched_yield() {
        asm("int $0xFE"); // Request task switch now.
        return 0;
    }
}
