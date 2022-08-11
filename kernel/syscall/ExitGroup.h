//
// Created by sigsegv on 8/11/22.
//

#ifndef JEOKERNEL_EXITGROUP_H
#define JEOKERNEL_EXITGROUP_H

#include "SyscallHandler.h"

class ExitGroup : public Syscall {
public:
    ExitGroup(SyscallHandler &handler) : Syscall(handler, 231) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_EXITGROUP_H
