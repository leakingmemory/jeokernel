//
// Created by sigsegv on 8/10/23.
//

#ifndef JEOKERNEL_SYSCALL_STAT_H
#define JEOKERNEL_SYSCALL_STAT_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Stat : public Syscall, private SyscallAsyncThread {
public:
    Stat(SyscallHandler &handler) : Syscall(handler, 4), SyscallAsyncThread("[stat]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYSCALL_STAT_H
