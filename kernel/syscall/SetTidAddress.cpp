//
// Created by sigsegv on 7/17/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include "SetTidAddress.h"

int64_t SetTidAddress::Call(int64_t tidptr_i, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto tidptr = (uintptr_t) tidptr_i;

    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = current_task->get_resource<ProcThread>();

    process->SetTidAddress(tidptr);
    return process->getpid();
}