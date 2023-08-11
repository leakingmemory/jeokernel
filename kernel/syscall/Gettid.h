//
// Created by sigsegv on 8/11/23.
//

#ifndef JEOKERNEL_GETTID_H
#define JEOKERNEL_GETTID_H

#include "SyscallHandler.h"

class Gettid : public Syscall {
public:
    Gettid(SyscallHandler &handler) : Syscall(handler, 186) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETTID_H
