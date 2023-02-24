//
// Created by sigsegv on 2/24/23.
//

#ifndef JEOKERNEL_FSTAT_H
#define JEOKERNEL_FSTAT_H

#include "SyscallHandler.h"

class Fstat : public Syscall {
public:
    Fstat(SyscallHandler &handler) : Syscall(handler, 5) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_FSTAT_H
