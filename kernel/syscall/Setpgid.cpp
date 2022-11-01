//
// Created by sigsegv on 10/31/22.
//

#include "Setpgid.h"
#include <exec/procthread.h>

int64_t Setpgid::Call(int64_t pid, int64_t pgid, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    return process->setpgid(pid, pgid);
}