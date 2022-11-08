//
// Created by sigsegv on 11/4/22.
//

#include "Clone.h"
#include "SyscallCtx.h"
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>

int64_t Clone::Call(int64_t flags, int64_t new_stackp, int64_t uptr_parent_tidptr, int64_t uptr_child_tidptr, SyscallAdditionalParams &params) {
    auto tls = params.Param5();
    std::cout << "clone(" << std::hex << flags << ", " << new_stackp << ", " << uptr_parent_tidptr << ", " << uptr_child_tidptr
    << ", " << tls << std::dec << ")\n";
    if ((flags & CloneSupportedFlags) != flags) {
        return -EOPNOTSUPP;
    }
    SyscallCtx ctx{params};
    std::shared_ptr<Process> clonedProcess = ctx.GetProcess().Clone();
    return -EOPNOTSUPP;
}