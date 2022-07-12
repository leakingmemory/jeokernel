//
// Created by sigsegv on 7/11/22.
//

#ifndef JEOKERNEL_GETUID_H
#define JEOKERNEL_GETUID_H

#include "SyscallHandler.h"

class Getuid : public Syscall {
public:
    Getuid(SyscallHandler &handler) : Syscall(handler, 102) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETUID_H
