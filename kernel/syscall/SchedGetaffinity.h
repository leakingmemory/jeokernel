//
// Created by sigsegv on 2/22/23.
//

#ifndef JEOKERNEL_SCHEDGETAFFINITY_H
#define JEOKERNEL_SCHEDGETAFFINITY_H

#include "SyscallHandler.h"

class SchedGetaffinity : public Syscall {
public:
    SchedGetaffinity(SyscallHandler &handler) : Syscall(handler, 204) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SCHEDGETAFFINITY_H
