//
// Created by sigsegv on 8/10/23.
//

#include <exec/procthread.h>
#include "SyscallCtx.h"
#include "Open.h"

int64_t Open::Call(int64_t dfd, int64_t uptr_filename, int64_t flags, int64_t mode, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};

    auto task_id = get_scheduler()->get_current_task_id();

    return ctx.ReadString(uptr_filename, [this, ctx, task_id, dfd, flags, mode] (const std::string &u_filename) {
        std::string filename{u_filename};

        Queue(task_id, [ctx, filename, dfd, flags, mode] () mutable {
            auto res = DoOpenAt(ctx.GetProcess(), dfd, filename, flags, mode);
            ctx.ReturnWhenNotRunning(res);
        });
        return resolve_return_value::AsyncReturn();
    });
}