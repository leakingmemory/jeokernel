//
// Created by sigsegv on 7/11/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include "Getgid.h"

int64_t Getgid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    return process->getgid();
}