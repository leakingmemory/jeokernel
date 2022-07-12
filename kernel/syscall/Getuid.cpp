//
// Created by sigsegv on 7/11/22.
//

#include <core/scheduler.h>
#include <exec/process.h>
#include "Getuid.h"

int64_t Getuid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    Process *process = current_task->get_resource<Process>();
    return process->getuid();
}