//
// Created by sigsegv on 8/10/23.
//

#include <exec/procthread.h>
#include <fcntl.h>
#include "SyscallCtx.h"
#include "Open.h"

int64_t Open::Call(int64_t uptr_filename, int64_t flags, int64_t mode, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};

    auto task_id = get_scheduler()->get_current_task_id();

    return ctx.ReadString(uptr_filename, [this, ctx, task_id, flags, mode] (const std::string &u_filename) {
        std::string filename{u_filename};

        Queue(task_id, [ctx, filename, flags, mode] () mutable {
            auto res = DoOpenAt(ctx.GetProcess(), AT_FDCWD, filename, flags, mode);
            ctx.ReturnWhenNotRunning(res);
        });
        return resolve_return_value::AsyncReturn();
    });
}