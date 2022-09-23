//
// Created by sigsegv on 9/22/22.
//

#ifndef JEOKERNEL_GETTIMEOFDAY_H
#define JEOKERNEL_GETTIMEOFDAY_H

#include "SyscallHandler.h"

struct timeval;
struct timezone;

class Gettimeofday : public Syscall {
public:
    Gettimeofday(SyscallHandler &handler) : Syscall(handler, 96) {}
    void DoGettimeofday(timeval *tv, timezone *tz);
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETTIMEOFDAY_H
