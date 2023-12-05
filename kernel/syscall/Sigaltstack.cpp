//
// Created by sigsegv on 8/21/23.
//

#include "Sigaltstack.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Sigaltstack::Call(int64_t uptr_ss, int64_t uptr_old_ss, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params, "Sigaltst"};
    return ctx.Read(uptr_ss, sizeof(struct sigaltstack), [ctx, uptr_old_ss] (void *ptr_ss) {
        auto *pss = (struct sigaltstack *) ptr_ss;
        auto ss = *pss;
        if (ss.ss_flags != 0 || ss.ss_sp == 0 || ss.ss_size == 0) {
            return resolve_return_value::Return(-EINVAL);
        }
        if (uptr_old_ss != 0) {
            return ctx.NestedWrite(uptr_old_ss, sizeof(struct sigaltstack), [ctx, ss] (void *ptr_old_ss) {
                auto *pold_ss = (struct sigaltstack *) ptr_old_ss;
                auto &old_ss = *pold_ss;
                ctx.GetProcess().SetSigaltstack(ss, old_ss);
                return resolve_return_value::Return(0);
            });
        } else {
            ctx.GetProcess().SetSigaltstack(ss);
            return resolve_return_value::Return(0);
        }
    });
}