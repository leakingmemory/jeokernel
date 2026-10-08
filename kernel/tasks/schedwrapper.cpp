//
// Created by sigsegv on 4/28/22.
//

#include <sched.h>

extern "C" {
    int sched_yield() {
        // TODO aarch64
#if !defined(__aarch64__)
        asm("int $0xFE"); // Request task switch now.
#endif
        return 0;
    }
}
