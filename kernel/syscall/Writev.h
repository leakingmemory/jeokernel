//
// Created by sigsegv on 7/25/22.
//

#ifndef JEOKERNEL_WRITEV_H
#define JEOKERNEL_WRITEV_H

#include "SyscallHandler.h"

class Writev : public Syscall {
public:
    Writev(SyscallHandler &handler) : Syscall(handler, 20) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_WRITEV_H
