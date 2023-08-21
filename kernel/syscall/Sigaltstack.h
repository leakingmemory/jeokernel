//
// Created by sigsegv on 8/21/23.
//

#ifndef JEOKERNEL_SIGALTSTACK_H
#define JEOKERNEL_SIGALTSTACK_H

#include "SyscallHandler.h"

class Sigaltstack : Syscall {
public:
    Sigaltstack(SyscallHandler &handler) : Syscall(handler, 131) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SIGALTSTACK_H
