//
// Created by sigsegv on 8/8/22.
//

#ifndef JEOKERNEL_FUTEX_H
#define JEOKERNEL_FUTEX_H

#include "SyscallHandler.h"

#define FUTEX_WAIT  0
#define FUTEX_WAKE  1

#define FUTEX_PRIVATE           0x080
#define FUTEX_CLOCK_REALTIME    0x100

#define FUTEX_FLAGS_MASK        (FUTEX_PRIVATE | FUTEX_CLOCK_REALTIME)
#define FUTEX_COMMAND_MASK      (~FUTEX_FLAGS_MASK)

class Futex : Syscall {
public:
    Futex(SyscallHandler &handler) : Syscall(handler, 202) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_FUTEX_H
