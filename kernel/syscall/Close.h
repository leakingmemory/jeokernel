//
// Created by sigsegv on 9/14/22.
//

#ifndef JEOKERNEL_CLOSE_H
#define JEOKERNEL_CLOSE_H

#include "SyscallHandler.h"

class Close : public Syscall {
public:
    Close(SyscallHandler &handler) : Syscall(handler, 3) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_CLOSE_H
