//
// Created by sigsegv on 7/17/22.
//

#ifndef JEOKERNEL_ARCHPRCTL_H
#define JEOKERNEL_ARCHPRCTL_H

#include "SyscallHandler.h"

class ArchPrctl : public Syscall {
public:
    ArchPrctl(SyscallHandler &handler) : Syscall(handler, 158) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_ARCHPRCTL_H
