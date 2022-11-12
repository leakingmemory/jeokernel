//
// Created by sigsegv on 11/12/22.
//

#ifndef JEOKERNEL_EXECVE_H
#define JEOKERNEL_EXECVE_H

#include "SyscallHandler.h"

class Execve : public Syscall{
public:
    Execve(SyscallHandler &handler) : Syscall(handler, 59) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_EXECVE_H
