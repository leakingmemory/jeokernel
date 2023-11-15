//
// Created by sigsegv on 9/14/22.
//

#include "Read.h"
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>
#include "SyscallCtx.h"

int64_t Read::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t, SyscallAdditionalParams &additionalParams) {
    SyscallCtx ctx{additionalParams};
    auto desc = ctx.GetProcess().get_file_descriptor(fd);
    if (!desc) {
        return -EBADF;
    }
    auto task_id = get_scheduler()->get_current_task_id();
    return ctx.Write(uptr_buf, count, [this, ctx, task_id, desc, count] (void *ptr) mutable {
        Queue(task_id, [ctx, desc, ptr, count] () mutable {
            std::shared_ptr<SyscallCtx> shctx = std::make_shared<SyscallCtx>(ctx);
            auto result = desc->read(shctx, ptr, count);
            if (result.hasValue) {
                ctx.ReturnWhenNotRunning(result.result);
            }
        });
        return resolve_return_value::AsyncReturn();
    });
}
