//
// Created by sigsegv on 10/31/22.
//

#ifndef JEOKERNEL_SETPGID_H
#define JEOKERNEL_SETPGID_H

#include "SyscallHandler.h"

class Setpgid : public Syscall {
public:
    Setpgid(SyscallHandler &handler) : Syscall(handler, 109) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SETPGID_H
