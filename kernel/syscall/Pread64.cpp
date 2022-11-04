//
// Created by sigsegv on 9/15/22.
//

#include "Pread64.h"
#include <exec/procthread.h>
#include <errno.h>
#include "SyscallCtx.h"

int64_t Pread64::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t pos, SyscallAdditionalParams &additionalParams) {
    SyscallCtx ctx{additionalParams};
    auto desc = ctx.GetProcess().get_file_descriptor(fd);
    if (!desc.Valid()) {
        return -EBADF;
    }
    return ctx.Write(uptr_buf, count, [this, ctx, desc, count, pos] (void *ptr) mutable {
        Queue([ctx, desc, ptr, count, pos] () mutable {
            std::shared_ptr<SyscallCtx> shctx = std::make_shared<SyscallCtx>(ctx);
            auto result = desc.read(shctx, ptr, count, pos);
            if (result.hasValue) {
                ctx.ReturnWhenNotRunning(result.result);
            }
        });
        return resolve_return_value::AsyncReturn();
    });
}