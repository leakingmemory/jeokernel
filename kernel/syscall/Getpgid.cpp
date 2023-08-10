//
// Created by sigsegv on 8/10/23.
//

#include <exec/procthread.h>
#include "Getpgid.h"

int64_t Getpgid::Call(int64_t pid, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    return process->getpgid((pid_t) pid);
}