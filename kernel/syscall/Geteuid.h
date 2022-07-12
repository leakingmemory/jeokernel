//
// Created by sigsegv on 7/11/22.
//

#ifndef JEOKERNEL_GETEUID_H
#define JEOKERNEL_GETEUID_H

#include "SyscallHandler.h"

class Geteuid : public Syscall {
public:
    Geteuid(SyscallHandler &handler) : Syscall(handler, 107) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETEUID_H
