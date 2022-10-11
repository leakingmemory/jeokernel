//
// Created by sigsegv on 10/11/22.
//

#ifndef JEOKERNEL_SYSCALL_TIME_H
#define JEOKERNEL_SYSCALL_TIME_H

#include "SyscallHandler.h"

class Time : public Syscall {
public:
    Time(SyscallHandler &handler) : Syscall(handler, 201) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYSCALL_TIME_H
