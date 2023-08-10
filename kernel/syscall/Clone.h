//
// Created by sigsegv on 11/4/22.
//

#ifndef JEOKERNEL_CLONE_H
#define JEOKERNEL_CLONE_H

#include "impl/SysCloneImpl.h"
#include "SyscallHandler.h"

class Clone : public Syscall, private SysCloneImpl {
public:
    Clone(SyscallHandler &handler) : Syscall(handler, 56) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_CLONE_H
