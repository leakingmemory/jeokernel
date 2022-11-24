//
// Created by sigsegv on 8/10/22.
//

#ifndef JEOKERNEL_STATX_H
#define JEOKERNEL_STATX_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Statx : public Syscall, private SyscallAsyncThread {
public:
    Statx(SyscallHandler &handler) : Syscall(handler, 332), SyscallAsyncThread("[statx]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_STATX_H
