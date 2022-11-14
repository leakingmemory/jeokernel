//
// Created by sigsegv on 11/12/22.
//

#ifndef JEOKERNEL_EXECVE_H
#define JEOKERNEL_EXECVE_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Execve : public Syscall, private SyscallAsyncThread {
public:
    Execve(SyscallHandler &handler) : Syscall(handler, 59), SyscallAsyncThread("[execve]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_EXECVE_H
