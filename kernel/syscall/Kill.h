//
// Created by sigsegv on 8/31/23.
//

#ifndef JEOKERNEL_KILL_H
#define JEOKERNEL_KILL_H

#include "SyscallHandler.h"

class Kill : public Syscall {
public:
    Kill(SyscallHandler &handler) : Syscall(handler, 62) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_KILL_H
