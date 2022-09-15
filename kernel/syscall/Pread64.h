//
// Created by sigsegv on 9/15/22.
//

#ifndef JEOKERNEL_PREAD64_H
#define JEOKERNEL_PREAD64_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Pread64 : public Syscall, private SyscallAsyncThread {
public:
    Pread64(SyscallHandler &handler) : Syscall(handler, 17), SyscallAsyncThread("[pread64]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_PREAD64_H
