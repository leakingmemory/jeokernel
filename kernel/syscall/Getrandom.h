//
// Created by sigsegv on 8/7/22.
//

#ifndef JEOKERNEL_GETRANDOM_H
#define JEOKERNEL_GETRANDOM_H

#include "SyscallHandler.h"

class Getrandom : public Syscall {
public:
    Getrandom(SyscallHandler &handler) : Syscall(handler, 318) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETRANDOM_H
