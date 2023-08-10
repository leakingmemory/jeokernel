//
// Created by sigsegv on 8/11/23.
//

#ifndef JEOKERNEL_FORK_H
#define JEOKERNEL_FORK_H

#include "SyscallHandler.h"
#include "impl/SysCloneImpl.h"

class Fork : public Syscall, private SysCloneImpl {
public:
    Fork(SyscallHandler &handler) : Syscall(handler, 57) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_FORK_H
