//
// Created by sigsegv on 8/7/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <time.h>
#include <errno.h>
#include "ClockGettime.h"

int64_t ClockGettime::Call(int64_t which, int64_t uptr_td, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = params.Scheduler();
    task *current_task = params.CurrentTask();
    auto *process = params.CurrentThread();
    auto result = process->resolve_read(uptr_td, sizeof(timespec), false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked("clockret", false);
        });
    }, [process, which, uptr_td](bool success, bool, std::function<void (intptr_t)>) {
        if (!success || !process->resolve_write(uptr_td, sizeof(timespec))) {
            return resolve_return_value::Return(-EFAULT);
        }
        UserMemory td_mem{*process, (uintptr_t) uptr_td, sizeof(timespec), true};
        int result = clock_gettime(which, (timespec *) td_mem.Pointer());
        return resolve_return_value::Return(result);
    });
    if (result.async) {
        current_task->set_blocked("clocktim", true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}