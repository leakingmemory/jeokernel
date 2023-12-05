//
// Created by sigsegv on 8/13/23.
//

#include "Membarrier.h"
#include "SyscallCtx.h"

int64_t Membarrier::Call(int64_t cmd, int64_t flags, int64_t cpu_id, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params, "Membarri"};
    return DoMembarrier(ctx.GetProcess(), (int) cmd, (unsigned int) flags, (int) cpu_id);
}
