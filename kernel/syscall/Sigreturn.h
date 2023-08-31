//
// Created by sigsegv on 8/30/23.
//

#ifndef JEOKERNEL_SIGRETURN_H
#define JEOKERNEL_SIGRETURN_H

#include "SyscallHandler.h"

class Sigreturn : public Syscall {
public:
    Sigreturn(SyscallHandler &handler) : Syscall(handler, 15) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SIGRETURN_H
