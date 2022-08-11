//
// Created by sigsegv on 8/7/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <time.h>
#include <errno.h>
#include "ClockGettime.h"

int64_t ClockGettime::Call(int64_t which, int64_t uptr_td, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    process->resolve_read(uptr_td, sizeof(timespec), [scheduler, current_task, process, which, uptr_td](bool success) {
        if (!success || !process->resolve_write(uptr_td, sizeof(timespec))) {
            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
            return;
        }
        UserMemory td_mem{*process, (uintptr_t) uptr_td, sizeof(timespec), true};
        int result = clock_gettime(which, (timespec *) td_mem.Pointer());
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
    return 0;
}