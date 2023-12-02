//
// Created by sigsegv on 9/9/22.
//

#include "OpenAt.h"
#include <exec/procthread.h>
#include "SyscallCtx.h"

int64_t OpenAt::Call(int64_t dfd, int64_t uptr_filename, int64_t flags, int64_t mode, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};

    auto task_id = params.TaskId();

    return ctx.ReadString(uptr_filename, [this, ctx, task_id, dfd, flags, mode] (const std::string &u_filename) {
        std::string filename{u_filename};

        Queue(task_id, [ctx, filename, dfd, flags, mode] () mutable {
            auto res = DoOpenAt(ctx.GetProcess(), dfd, filename, flags, mode);
            ctx.ReturnWhenNotRunning(res);
        });
        return resolve_return_value::AsyncReturn();
    });
}