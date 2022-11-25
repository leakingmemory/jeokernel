//
// Created by sigsegv on 9/22/22.
//

#include "Gettimeofday.h"
#include <time.h>
#include "SyscallCtx.h"

void Gettimeofday::DoGettimeofday(timeval *tv, timezone *tz) {
    clock_gettimeofday(tv, tz);
}

int64_t Gettimeofday::Call(int64_t uptr_tv, int64_t uptr_tz, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    if (uptr_tv != 0) {
        return ctx.Write(uptr_tv, sizeof(timeval), [this, ctx, uptr_tz] (void *ptr_tv) {
            if (uptr_tz != 0) {
                return ctx.NestedWrite(uptr_tz, sizeof(timezone), [this, ctx, ptr_tv] (void *ptr_tz) {
                    DoGettimeofday((timeval *) ptr_tv, (timezone *) ptr_tz);
                    return resolve_return_value::Return(0);
                });
            } else {
                DoGettimeofday((timeval *) ptr_tv, nullptr);
                return resolve_return_value::Return(0);
            }
        });
    } else if (uptr_tz != 0) {
        return ctx.Write(uptr_tz, sizeof(timezone), [this, ctx] (void *ptr_tz) {
            DoGettimeofday(nullptr, (timezone *) ptr_tz);
            return resolve_return_value::Return(0);
        });
    } else {
        return 0;
    }
}