//
// Created by sigsegv on 8/8/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <exec/futex.h>
#include <iostream>
#include <errno.h>
#include "Futex.h"
#include "SyscallCtx.h"

#define FUTEX_FLAGS_MASK        (FUTEX_PRIVATE | FUTEX_CLOCK_REALTIME)
#define FUTEX_COMMAND_MASK      (~FUTEX_FLAGS_MASK)

int64_t SysFutex::Call(int64_t uptr_addr, int64_t futex_op, int64_t val, int64_t uptr_timeout_or_val2, SyscallAdditionalParams &params) {
    uintptr_t uptr_addr2 = params.Param5();
    int64_t val3 = params.Param6();

    auto ctx = std::make_shared<SyscallCtx>(params, "Futex");
    auto op = futex_op & FUTEX_COMMAND_MASK;
    switch (op) {
        case FUTEX_WAKE: {
            uintptr_t wakeNum = ((uintptr_t) val) & 0x7FFFFFFFULL;
            bool wakeAll = wakeNum == 0x7FFFFFFF;
            if (wakeAll) {
                return Futex::Instance().WakeAll(*(ctx->GetProcess().GetProcess()), uptr_addr);
            } else {
                return Futex::Instance().Wake(*(ctx->GetProcess().GetProcess()), uptr_addr, wakeNum);
            }
            break;
        }
        case FUTEX_WAIT:
            return Futex::Instance().Wait(ctx, uptr_addr, val);
    }
    std::cerr << "not implemented futex(" << std::hex << ((uintptr_t) uptr_addr) << ", " << futex_op << ", " << val << ", "
              << uptr_timeout_or_val2 << ", " << uptr_addr2 << ", " << val3 << std::dec << ")\n";
    return -EOPNOTSUPP;
}
