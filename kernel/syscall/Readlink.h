//
// Created by sigsegv on 8/2/22.
//

#ifndef JEOKERNEL_READLINK_H
#define JEOKERNEL_READLINK_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class ProcThread;

class Readlink : public Syscall, private SyscallAsyncThread {
public:
    Readlink(SyscallHandler &handler) : Syscall(handler, 89), SyscallAsyncThread("[readlink]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_READLINK_H
