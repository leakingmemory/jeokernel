//
// Created by sigsegv on 7/11/22.
//

#ifndef JEOKERNEL_GETGID_H
#define JEOKERNEL_GETGID_H

#include "SyscallHandler.h"

class Getgid : public Syscall {
public:
    Getgid(SyscallHandler &handler) : Syscall(handler, 104) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETGID_H
