//
// Created by sigsegv on 8/12/23.
//

#ifndef JEOKERNEL_READV_H
#define JEOKERNEL_READV_H

#include "SyscallHandler.h"

class Readv : public Syscall {
public:
    Readv(SyscallHandler &handler) : Syscall(handler, 19) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_READV_H
