//
// Created by sigsegv on 11/11/22.
//

#include "Wait4.h"
#include <iostream>
#include <errno.h>
#include "SyscallCtx.h"
#include <sys/types.h>
#include <exec/procthread.h>

//#define DEBUG_WAIT_CALL

resolve_return_value Wait4::WaitAny(SyscallCtx &r_ctx, int *status) {
    SyscallCtx ctx{r_ctx};
    child_result result{};
    bool sync = ctx.GetProcess().WaitForAnyChild(result, [ctx, status] (pid_t pid, intptr_t result) {
        if (status != nullptr) {
            *status = (int) result;
        }
        ctx.ReturnWhenNotRunning(pid);
    });
    if (sync) {
        if (status != nullptr) {
            *status = (int) result.result;
        }
        return resolve_return_value::Return(result.pid);
    }
    return resolve_return_value::AsyncReturn();
}

int64_t Wait4::Call(int64_t u_pid, int64_t uptr_stat_addr, int64_t options, int64_t uptr_rusage, SyscallAdditionalParams &params) {
    pid_t pid = (pid_t) u_pid;
    if (pid == -1) {
#ifdef DEBUG_WAIT_CALL
        std::cout << "wait4(" << std::dec << pid << "/any, " << std::hex << uptr_stat_addr << ", " << options << ", " << uptr_rusage << ")\n";
#endif
        SyscallCtx ctx{params, "Wait4"};
        if (uptr_stat_addr != 0) {
            return ctx.Write(uptr_stat_addr, sizeof(int), [this, ctx] (void *ptr) mutable {
                return WaitAny(ctx, (int *) ptr);
            });
        }
        auto result = WaitAny(ctx, nullptr);
        if (result.hasValue) {
            return result.result;
        }
        ctx.AsyncCtx()->async();
        return 0;
    }
    std::cout << "wait4(" << std::dec << pid << ", " << std::hex << uptr_stat_addr << ", " << options << ", " << uptr_rusage << ") not implemented\n";
    return -EOPNOTSUPP;
}
