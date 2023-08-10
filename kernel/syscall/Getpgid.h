//
// Created by sigsegv on 8/10/23.
//

#ifndef JEOKERNEL_GETPGID_H
#define JEOKERNEL_GETPGID_H

#include "SyscallHandler.h"

class Getpgid : public Syscall {
public:
    Getpgid(SyscallHandler &handler) : Syscall(handler, 121) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETPGID_H
