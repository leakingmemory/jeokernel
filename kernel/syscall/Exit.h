//
// Created by sigsegv on 6/18/22.
//

#ifndef JEOKERNEL_EXIT_H
#define JEOKERNEL_EXIT_H

#include "SyscallHandler.h"

class Exit : public Syscall {
public:
    Exit(SyscallHandler &handler) : Syscall(handler, 0x3C) {
    }
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &additionalParams) override;
};


#endif //JEOKERNEL_EXIT_H
