//
// Created by sigsegv on 7/11/22.
//

#ifndef JEOKERNEL_GETEGID_H
#define JEOKERNEL_GETEGID_H

#include "SyscallHandler.h"

class Getegid : public Syscall {
public:
    Getegid(SyscallHandler &handler) : Syscall(handler, 108) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETEGID_H
