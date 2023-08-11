//
// Created by sigsegv on 8/11/23.
//

#include "Gettid.h"
#include <exec/procthread.h>

int64_t Gettid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    return process->getpid();
}