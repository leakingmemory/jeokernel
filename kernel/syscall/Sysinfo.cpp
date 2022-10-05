//
// Created by sigsegv on 10/5/22.
//

#include "Sysinfo.h"
#include "SyscallCtx.h"
#include <sys/sysinfo.h>
#include <core/nanotime.h>

int64_t Sysinfo::Call(int64_t uptr, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return ctx.Write(uptr, sizeof(struct sysinfo), [ctx] (void *ptr) mutable {
        auto *info = (struct sysinfo *) ptr;
        {
            auto nt = get_nanotime_ref();
            nt /= 1000000000;
            info->uptime = nt;
        }
        info->loads[0] = 0;
        info->loads[1] = 0;
        info->loads[2] = 0;
        info->procs = 0;
        physmem_stats(*info);
        info->totalswap = 0;
        info->freeswap = 0;
        info->sharedram = 0;
        info->bufferram = 0;
        return ctx.Return(0);
    });
}