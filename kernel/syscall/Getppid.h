//
// Created by sigsegv on 10/10/22.
//

#ifndef JEOKERNEL_GETPPID_H
#define JEOKERNEL_GETPPID_H

#include "SyscallHandler.h"

class Getppid : public Syscall {
public:
    Getppid(SyscallHandler &handler) : Syscall(handler, 110) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETPPID_H
