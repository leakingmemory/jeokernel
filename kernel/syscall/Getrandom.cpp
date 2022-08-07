//
// Created by sigsegv on 8/7/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <hash/random.h>
#include "Getrandom.h"

int64_t Getrandom::Call(int64_t uptr_buf, int64_t len, int64_t flags, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = current_task->get_resource<ProcThread>();
    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    process->resolve_read(uptr_buf, len, [scheduler, current_task, process, uptr_buf, len](bool success) {
        if (!success || !process->resolve_write(uptr_buf, len)) {
            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
            return;
        }
        UserMemory buf_mem{*process, (uintptr_t) uptr_buf, (uintptr_t) len, true};
        GetRandom(buf_mem.Pointer(), len);
        scheduler->when_not_running(*current_task, [current_task] () {
            current_task->get_cpu_state().rax = (uint64_t) 0;
            current_task->set_blocked(false);
        });
    });
    return 0;
}