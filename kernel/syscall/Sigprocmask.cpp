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
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    current_task->set_blocked(true);
    process->resolve_read(uptrset, sizeof(sigset_t), [scheduler, current_task, process, how, uptrset, uptroldset, sigsetsize] (bool success) {
        int64_t result{0};
        {
            UserMemory uptrset_mem{*process, (uintptr_t) uptrset, (uintptr_t) sigsetsize};
            UserMemory uptroldset_mem{*process, (uintptr_t) uptroldset, (uintptr_t) sigsetsize, true};
            if ((!uptrset_mem && uptrset != 0) || (!uptroldset_mem && uptroldset != 0)) {
                std::cerr << "sigprocmask: " << std::hex << (uintptr_t) uptrset << ", " << (uintptr_t) uptroldset << ", " << sigsetsize << std::dec << " memfault\n";
                result = -EFAULT;
                goto done_sigprocmask;
            }
            process->sigprocmask(how, uptrset != 0 ? (const sigset_t *) uptrset_mem.Pointer() : nullptr, uptroldset != 0 ? (sigset_t *) uptroldset_mem.Pointer() : nullptr, sigsetsize);
        }
done_sigprocmask:
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
    params.DoContextSwitch(true);
    return 0;
}