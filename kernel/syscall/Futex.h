//
// Created by sigsegv on 8/8/22.
//

#ifndef JEOKERNEL_SYS_FUTEX_H
#define JEOKERNEL_SYS_FUTEX_H

#include <sys/types.h>
#include "SyscallHandler.h"

class SysFutex : public Syscall {
public:
    SysFutex(SyscallHandler &handler) : Syscall(handler, 202) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYS_FUTEX_H
