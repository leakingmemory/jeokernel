//
// Created by sigsegv on 8/7/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <hash/random.h>
#include "Getrandom.h"

int64_t Getrandom::Call(int64_t uptr_buf, int64_t len, int64_t flags, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = params.Scheduler();
    task *current_task = params.CurrentTask();
    auto *process = params.CurrentThread();
    auto result = process->resolve_read(uptr_buf, len, false, [scheduler, current_task] (uintptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked("grandret", false);
        });
    }, [process, uptr_buf, len](bool success, bool, const std::function<void (uintptr_t)> &) {
        if (!success || !process->resolve_write(uptr_buf, len)) {
            return resolve_return_value::Return(-EFAULT);
        }
        UserMemory buf_mem{*process, (uintptr_t) uptr_buf, (uintptr_t) len, true};
        GetRandom(buf_mem.Pointer(), len);
        return resolve_return_value::Return(0);
    });
    if (result.async) {
        current_task->set_blocked("gerandom", true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}