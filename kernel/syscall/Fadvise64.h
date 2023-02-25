//
// Created by sigsegv on 2/25/23.
//

#ifndef JEOKERNEL_FADVISE64_H
#define JEOKERNEL_FADVISE64_H

#include "SyscallHandler.h"

class Fadvise64 : public Syscall {
public:
    Fadvise64(SyscallHandler &handler) : Syscall(handler, 221) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_FADVISE64_H
