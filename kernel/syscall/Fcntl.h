//
// Created by sigsegv on 10/11/22.
//

#ifndef JEOKERNEL_SYSCALL_FCNTL_H
#define JEOKERNEL_SYSCALL_FCNTL_H

#include "SyscallHandler.h"

class Fcntl : public Syscall {
public:
    Fcntl(SyscallHandler &handler) : Syscall(handler, 72) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYSCALL_FCNTL_H
