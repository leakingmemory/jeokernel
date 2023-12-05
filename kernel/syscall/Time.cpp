//
// Created by sigsegv on 10/11/22.
//

#include "Time.h"
#include <time.h>
#include "SyscallCtx.h"

int64_t Time::Call(int64_t uptr, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    struct timeval tv;
    clock_gettimeofday(&tv, nullptr);
    if (uptr != 0) {
        SyscallCtx ctx{params, "Time"};
        return ctx.Write(uptr, sizeof(time_t), [ctx, tv] (void *ptr) {
            time_t &val = *((time_t *) ptr);
            val = tv.tv_sec;
            return ctx.Return(tv.tv_sec);
        });
    }
    return tv.tv_sec;
}