//
// Created by sigsegv on 8/13/23.
//

#include <exec/procthread.h>
#include "Faccessat2.h"
#include "SyscallCtx.h"

int64_t Faccessat2::Call(int64_t dfd, int64_t uptr_filename, int64_t mode, int64_t flags, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};

    auto task_id = get_scheduler()->get_current_task_id();

    return ctx.ReadString(uptr_filename, [this, ctx, dfd, mode, flags, task_id] (const std::string &u_filename) {
        std::string filename{u_filename};

        ctx.GetProcess().QueueBlocking(task_id, [this, ctx, filename, dfd, mode, flags] () mutable {
            auto res = DoFaccessat(ctx.GetProcess(), dfd, filename, mode, flags);
            ctx.ReturnWhenNotRunning(res);
        });
        return resolve_return_value::AsyncReturn();
    });
}