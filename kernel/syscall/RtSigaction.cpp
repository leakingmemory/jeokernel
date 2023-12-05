//
// Created by sigsegv on 10/6/22.
//

#include "RtSigaction.h"
#include "SyscallCtx.h"
#include <signal.h>
#include <errno.h>
#include <exec/procthread.h>
#include <iostream>

//#define DEBUG_RTSIGACTION
//#define DEBUG_RTSIGACTION_RAW

int RtSigaction::DoRtSigaction(ProcThread &process, int signal, const sigaction *act, sigaction *oact) {
#ifdef DEBUG_RTSIGACTION
    std::cout << "sigaction(" << std::dec << signal << ", 0x" << std::hex << (uintptr_t) act << ", 0x" << (uintptr_t) oact << std::dec << ") -> "
        << std::hex << (act != nullptr ? (uintptr_t) act->sa_handler : 0) << " flags " << (act != nullptr ? act->sa_flags : 0) << std::dec << " => ";
    auto rval =
#else
    return
#endif
        process.sigaction(signal, act, oact);
#ifdef DEBUG_RTSIGACTION
    std::cout << rval << "\n";
    return rval;
#endif
}

int64_t RtSigaction::Call(int64_t signal, int64_t uptr_act, int64_t uptr_oact, int64_t sigsetsize, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params, "RtSigact"};
#ifdef DEBUG_RTSIGACTION_RAW
    std::cout << "sigaction(" << std::dec << signal << ", 0x" << std::hex << (uintptr_t) uptr_act << ", 0x" << (uintptr_t) uptr_oact << std::dec << ", " << sigsetsize << ")\n";
#endif
    if (sigsetsize != sizeof(sigset_t)) {
        return -EINVAL;
    }
    if (uptr_act != 0) {
        if (uptr_oact != 0) {
            return ctx.Read(uptr_act, sizeof(sigaction), [this, ctx, uptr_oact, signal] (void *ptr_act) {
                return ctx.NestedWrite(uptr_oact, sizeof(sigaction), [this, ctx, ptr_act, signal] (void *ptr_oact) {
                    return ctx.Return(DoRtSigaction(ctx.GetProcess(), signal, (const sigaction *) ptr_act, (sigaction *) ptr_oact));
                });
            });
        } else {
            return ctx.Read(uptr_act, sizeof(sigaction), [this, ctx, signal] (void *ptr_act) {
                return ctx.Return(DoRtSigaction(ctx.GetProcess(), signal, (const sigaction *) ptr_act, nullptr));
            });
        }
    } else if (uptr_oact != 0) {
        return ctx.Write(uptr_oact, sizeof(sigaction), [this, ctx, signal] (void *ptr_oact) {
            return ctx.Return(DoRtSigaction(ctx.GetProcess(), signal, nullptr, (sigaction *) ptr_oact));
        });
    } else {
        return 0;
    }
}
