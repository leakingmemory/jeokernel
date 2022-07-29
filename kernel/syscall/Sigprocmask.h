//
// Created by sigsegv on 7/27/22.
//

#ifndef JEOKERNEL_SIGPROCMASK_H
#define JEOKERNEL_SIGPROCMASK_H

#include "SyscallHandler.h"

class Sigprocmask : public Syscall {
public:
    Sigprocmask(SyscallHandler &handler) : Syscall(handler, 14) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SIGPROCMASK_H
