//
// Created by sigsegv on 7/27/22.
//

#include <iostream>
#include <signal.h>
#include <errno.h>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include "Sigprocmask.h"

int64_t Sigprocmask::Call(int64_t how, int64_t uptrset, int64_t uptroldset, int64_t sigsetsize, SyscallAdditionalParams &params) {
    auto *scheduler = params.Scheduler();
    task *current_task = params.CurrentTask();
    auto *process = params.CurrentThread();
    auto result = process->resolve_read(uptrset, sizeof(sigset_t), false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    }, [process, how, uptrset, uptroldset, sigsetsize] (bool success, bool, const std::function<void (intptr_t)> &) {
        UserMemory uptrset_mem{*process, (uintptr_t) uptrset, (uintptr_t) sigsetsize};
        UserMemory uptroldset_mem{*process, (uintptr_t) uptroldset, (uintptr_t) sigsetsize, true};
        if ((!uptrset_mem && uptrset != 0) || (!uptroldset_mem && uptroldset != 0)) {
            std::cerr << "sigprocmask: " << std::hex << (uintptr_t) uptrset << ", " << (uintptr_t) uptroldset << ", " << sigsetsize << std::dec << " memfault\n";
            return resolve_return_value::Return(-EFAULT);
        }
        process->sigprocmask(how, uptrset != 0 ? (const sigset_t *) uptrset_mem.Pointer() : nullptr, uptroldset != 0 ? (sigset_t *) uptroldset_mem.Pointer() : nullptr, sigsetsize);
        return resolve_return_value::Return(0);
    });
    if (result.async) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}