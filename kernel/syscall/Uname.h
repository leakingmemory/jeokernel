//
// Created by sigsegv on 7/22/22.
//

#ifndef JEOKERNEL_UNAME_H
#define JEOKERNEL_UNAME_H

#include "SyscallHandler.h"

class Uname : public Syscall {
public:
    Uname(SyscallHandler &handler) : Syscall(handler, 63) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_UNAME_H
