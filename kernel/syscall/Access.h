//
// Created by sigsegv on 9/8/22.
//

#ifndef JEOKERNEL_ACCESS_H
#define JEOKERNEL_ACCESS_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class ProcThread;

class Access : public Syscall {
public:
    Access(SyscallHandler &handler) : Syscall(handler, 21) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_ACCESS_H
