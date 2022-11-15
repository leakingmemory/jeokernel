//
// Created by sigsegv on 11/15/22.
//

#ifndef JEOKERNEL_SYSCALL_PRCTL_H
#define JEOKERNEL_SYSCALL_PRCTL_H

#include "SyscallHandler.h"

class Prctl : Syscall {
public:
    Prctl(SyscallHandler &handler) : Syscall(handler, 157) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYSCALL_PRCTL_H
