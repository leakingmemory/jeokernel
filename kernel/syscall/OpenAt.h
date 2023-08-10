//
// Created by sigsegv on 9/9/22.
//

#ifndef JEOKERNEL_OPENAT_H
#define JEOKERNEL_OPENAT_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"
#include "impl/SysOpenImpl.h"

class ProcThread;

class OpenAt : public Syscall, private SyscallAsyncThread, private SysOpenImpl {
public:
    OpenAt(SyscallHandler &handler) : Syscall(handler, 257), SyscallAsyncThread("[openat]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_OPENAT_H
