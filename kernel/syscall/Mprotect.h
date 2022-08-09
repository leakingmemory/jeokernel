//
// Created by sigsegv on 8/9/22.
//

#ifndef JEOKERNEL_MPROTECT_H
#define JEOKERNEL_MPROTECT_H

#include "SyscallHandler.h"

class Mprotect : public Syscall {
public:
    Mprotect(SyscallHandler &handler) : Syscall(handler, 10) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_MPROTECT_H
