//
// Created by sigsegv on 8/2/22.
//

#ifndef JEOKERNEL_READLINK_H
#define JEOKERNEL_READLINK_H

#include "SyscallHandler.h"

class Readlink : public Syscall {
public:
    Readlink(SyscallHandler &handler) : Syscall(handler, 89) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_READLINK_H
