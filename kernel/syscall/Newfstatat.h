//
// Created by sigsegv on 8/10/22.
//

#ifndef JEOKERNEL_NEWFSTATAT_H
#define JEOKERNEL_NEWFSTATAT_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Newfstatat : public Syscall, private SyscallAsyncThread {
public:
    Newfstatat(SyscallHandler &handler) : Syscall(handler, 262), SyscallAsyncThread("[newfstatat]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_NEWFSTATAT_H
