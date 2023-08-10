//
// Created by sigsegv on 8/10/23.
//

#ifndef JEOKERNEL_OPEN_H
#define JEOKERNEL_OPEN_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"
#include "impl/SysOpenImpl.h"

class Open : public Syscall, private SyscallAsyncThread, private SysOpenImpl {
public:
    Open(SyscallHandler &handler) : Syscall(handler, 2), SyscallAsyncThread("[open]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_OPEN_H
