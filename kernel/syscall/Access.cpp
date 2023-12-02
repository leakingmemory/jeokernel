//
// Created by sigsegv on 9/8/22.
//

#include "Access.h"
#include <exec/procthread.h>
#include <fcntl.h>
#include "SyscallCtx.h"
#include "impl/SysFaccessatImpl.h"

//#define DEBUG_ACCESS_CALL

int64_t Access::Call(int64_t uptr_filename, int64_t mode, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};

    auto task_id = params.Scheduler()->get_current_task_id();

    return ctx.ReadString(uptr_filename, [this, ctx, task_id, mode] (const std::string &u_filename) {
        std::string filename{u_filename};

        ctx.GetProcess().QueueBlocking(task_id, [this, ctx, filename, mode] () mutable {
            auto res = SysFaccessatImpl::Create()->DoFaccessat(ctx.GetProcess(), AT_FDCWD, filename, mode, 0);
#ifdef DEBUG_ACCESS_CALL
            std::cout << "access(" << filename << ", " << std::hex << mode << std::dec << ") => " << res << "\n";
#endif
            ctx.ReturnWhenNotRunning(res);
        });
        return resolve_return_value::AsyncReturn();
    });
}