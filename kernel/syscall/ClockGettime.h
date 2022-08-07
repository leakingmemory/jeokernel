//
// Created by sigsegv on 8/7/22.
//

#ifndef JEOKERNEL_CLOCKGETTIME_H
#define JEOKERNEL_CLOCKGETTIME_H

#include "SyscallHandler.h"

class ClockGettime : public Syscall {
public:
    ClockGettime(SyscallHandler &handler) : Syscall(handler, 228) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_CLOCKGETTIME_H
