//
// Created by sigsegv on 8/10/22.
//

#ifndef JEOKERNEL_NEWFSTATAT_H
#define JEOKERNEL_NEWFSTATAT_H

#include "SyscallHandler.h"

class Newfstatat : public Syscall {
public:
    Newfstatat(SyscallHandler &handler) : Syscall(handler, 262) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_NEWFSTATAT_H
