//
// Created by sigsegv on 8/17/23.
//

#ifndef JEOKERNEL_NANOSLEEP_H
#define JEOKERNEL_NANOSLEEP_H

#include <memory>
#include "SyscallHandler.h"

struct timespec;
class SyscallCtx;
class task;

class Nanosleep : public Syscall {
public:
    Nanosleep(SyscallHandler &handler) : Syscall(handler, 35) {}
private:
    static void DoNanosleep(std::shared_ptr<SyscallCtx> ctx, task &, const timespec *request, timespec *remaining);
public:
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_NANOSLEEP_H
