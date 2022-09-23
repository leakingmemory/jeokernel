//
// Created by sigsegv on 9/23/22.
//

#ifndef JEOKERNEL_IOCTL_H
#define JEOKERNEL_IOCTL_H

#include "SyscallHandler.h"

class Ioctl : public Syscall {
public:
    Ioctl(SyscallHandler &handler) : Syscall(handler, 16) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_IOCTL_H
