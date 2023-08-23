//
// Created by sigsegv on 8/22/23.
//

#ifndef JEOKERNEL_SYS_PIPE_H
#define JEOKERNEL_SYS_PIPE_H

#include "SyscallHandler.h"

class Pipe : public Syscall {
public:
    Pipe(SyscallHandler &handler) : Syscall(handler, 22) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYS_PIPE_H
