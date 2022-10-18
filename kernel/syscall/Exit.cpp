//
// Created by sigsegv on 6/18/22.
//

#include <core/scheduler.h>
#include "Exit.h"
#include <exec/procthread.h>

int64_t Exit::Call(int64_t returnValue, int64_t, int64_t, int64_t, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    process->SetExitCode(returnValue);
    scheduler->user_exit(returnValue);
    additionalParams.DoContextSwitch(true);
    return 0;
}