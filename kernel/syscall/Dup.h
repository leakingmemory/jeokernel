//
// Created by sigsegv on 10/10/22.
//

#ifndef JEOKERNEL_DUP_H
#define JEOKERNEL_DUP_H

#include "SyscallHandler.h"

class Dup : public Syscall {
public:
    Dup(SyscallHandler &handler) : Syscall(handler, 32) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_DUP_H
