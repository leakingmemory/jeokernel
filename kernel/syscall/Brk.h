//
// Created by sigsegv on 7/12/22.
//

#ifndef JEOKERNEL_BRK_H
#define JEOKERNEL_BRK_H

#include "SyscallHandler.h"

class Brk : public Syscall {
public:
    Brk(SyscallHandler &handler) : Syscall(handler, 12) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_BRK_H
