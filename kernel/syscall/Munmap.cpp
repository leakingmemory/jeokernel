//
// Created by sigsegv on 12/5/22.
//

#include "Munmap.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>

int64_t Munmap::Call(int64_t uptr_addr, int64_t len, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    auto pages = len >> 12;
    if (pages == 0) {
        return 0;
    }
    auto pagenum = uptr_addr >> 12;
    auto &proc = ctx.GetProcess();
    auto discard = proc.ClearRange(pagenum, pages);
    proc.DisableRange(pagenum, pages);
    discard.clear();
    return 0;
}