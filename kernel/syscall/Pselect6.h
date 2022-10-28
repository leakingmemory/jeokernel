//
// Created by sigsegv on 10/11/22.
//

#ifndef JEOKERNEL_PSELECT6_H
#define JEOKERNEL_PSELECT6_H

#include "SyscallHandler.h"
#include "SyscallCtx.h"
#include <signal.h>
#include "SyscallAsyncThread.h"

struct fdset;
struct timespec;

class Pselect6 : public Syscall, private SyscallAsyncThread {
public:
    Pselect6(SyscallHandler &handler) : Syscall(handler, 270), SyscallAsyncThread("[pselect6]") {}
    resolve_return_value DoSelect(SyscallCtx ctx, int n, fdset *inp, fdset *outp, fdset *exc, const timespec *timeout, const sigset_t *sigset);
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_PSELECT6_H
