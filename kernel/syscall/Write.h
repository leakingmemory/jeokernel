//
// Created by sigsegv on 6/19/22.
//

#ifndef JEOKERNEL_WRITE_H
#define JEOKERNEL_WRITE_H

#include "SyscallHandler.h"

class Write : public Syscall {
public:
    Write(SyscallHandler &handler) : Syscall(handler, 1) {
    }
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &additionalParams) override;
};


#endif //JEOKERNEL_WRITE_H
