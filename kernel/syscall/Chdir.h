//
// Created by sigsegv on 12/5/22.
//

#ifndef JEOKERNEL_CHDIR_H
#define JEOKERNEL_CHDIR_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Process;

class Chdir : public Syscall, SyscallAsyncThread {
public:
    Chdir(SyscallHandler &handler) : Syscall(handler, 80), SyscallAsyncThread("[chdir]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_CHDIR_H
