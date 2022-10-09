//
// Created by sigsegv on 10/9/22.
//

#ifndef JEOKERNEL_GETPID_H
#define JEOKERNEL_GETPID_H

#include "SyscallHandler.h"

class Getpid : public Syscall {
public:
    Getpid(SyscallHandler &handler) : Syscall(handler, 39) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETPID_H
