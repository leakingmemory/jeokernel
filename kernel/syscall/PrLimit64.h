//
// Created by sigsegv on 8/1/22.
//

#ifndef JEOKERNEL_PRLIMIT64_H
#define JEOKERNEL_PRLIMIT64_H

#include "SyscallHandler.h"

class PrLimit64 : Syscall {
public:
    PrLimit64(SyscallHandler &handler) : Syscall(handler, 302) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_PRLIMIT64_H
