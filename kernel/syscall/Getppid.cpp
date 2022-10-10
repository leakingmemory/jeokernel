//
// Created by sigsegv on 10/10/22.
//

#include "Getppid.h"
#include <exec/procthread.h>

int64_t Getppid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    return process->getppid();
}