//
// Created by sigsegv on 8/12/22.
//

#include <exec/procthread.h>
#include <exec/rseq.h>
#include <exec/usermem.h>
#include <errno.h>
#include "Rseq.h"

int64_t Rseq::Call(int64_t uptr_rseq, int64_t rseq_len32, int64_t i_flags, int64_t u32_sig, SyscallAdditionalParams &params) {
    uint32_t rseq_len = (uint32_t) rseq_len32;
    int flags = (int) i_flags;
    uint32_t sig = u32_sig;
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    if (rseq_len != sizeof(struct rseq)) {
        return -EINVAL;
    }
    if ((flags & RSEQ_FLAG_UNREGISTER) != 0) {
        if (flags != RSEQ_FLAG_UNREGISTER) {
            return -EINVAL;
        }
        if (uptr_rseq == 0 || process->RSeq().U_Rseq() != uptr_rseq || process->RSeq().Sig() != sig) {
            return -EINVAL;
        }
        return process->RSeq().Unregister();
    }
    if (flags != 0) {
        return -EINVAL;
    }
    if (process->RSeq().IsRegistered()) {
        if (uptr_rseq != process->RSeq().U_Rseq()) {
            return -EINVAL;
        }
        if (sig != process->RSeq().Sig()) {
            return -EPERM;
        }
        return -EBUSY;
    }
    auto result = process->resolve_read(uptr_rseq, rseq_len, false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    }, [process, uptr_rseq, rseq_len, sig] (bool success, bool, const std::function<void (intptr_t)> &) {
        if (!success || !process->resolve_write(uptr_rseq, rseq_len)) {
            return resolve_return_value::Return(-EFAULT);
        }
        UserMemory kmem{*process, (uintptr_t) uptr_rseq, rseq_len, true};
        if (!kmem) {
            return resolve_return_value::Return(-EFAULT);
        }
        auto result = process->RSeq().Register(uptr_rseq, *((rseq *) kmem.Pointer()), sig);
        return resolve_return_value::Return(result);
    });
    if (result.async) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}