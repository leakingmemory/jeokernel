//
// Created by sigsegv on 11/11/22.
//

#ifndef JEOKERNEL_WAIT4_H
#define JEOKERNEL_WAIT4_H

#include "SyscallHandler.h"
#include <exec/resolve_return.h>

class SyscallCtx;

class Wait4 : Syscall {
public:
    Wait4(SyscallHandler &handler) : Syscall(handler, 61) {}
private:
    resolve_return_value WaitAny(SyscallCtx &ctx, int *status);
public:
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_WAIT4_H
