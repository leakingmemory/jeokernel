//
// Created by sigsegv on 11/4/22.
//

#ifndef JEOKERNEL_CLONE_H
#define JEOKERNEL_CLONE_H

#include "SyscallHandler.h"

#define CLONE_CHILD_SETTID          0x1000000
#define CLONE_CHILD_CLEARTID        0x0200000
#define CLONE_CHILD_EXIT_SIGNALMASK 0x00000FF

constexpr unsigned int CloneSupportedFlags = (
        CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | CLONE_CHILD_EXIT_SIGNALMASK
        );

class Clone : public Syscall {
public:
    Clone(SyscallHandler &handler) : Syscall(handler, 56) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_CLONE_H
