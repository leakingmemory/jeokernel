//
// Created by sigsegv on 3/6/23.
//

#ifndef JEOKERNEL_MKDIR_H
#define JEOKERNEL_MKDIR_H

#include "SyscallHandler.h"

class Mkdir : public Syscall {
public:
    Mkdir(SyscallHandler &handler) : Syscall(handler, 83) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_MKDIR_H
