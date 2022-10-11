//
// Created by sigsegv on 10/11/22.
//

#ifndef JEOKERNEL_DUP2_H
#define JEOKERNEL_DUP2_H

#include "SyscallHandler.h"

class Dup2 : public Syscall {
public:
    Dup2(SyscallHandler &handler) : Syscall(handler, 33) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_DUP2_H
