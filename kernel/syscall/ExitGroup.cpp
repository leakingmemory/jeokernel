//
// Created by sigsegv on 8/11/22.
//

//#define EXIT_GROUP_DEBUG

#include <exec/procthread.h>
#ifdef EXIT_GROUP_DEBUG
#include <iostream>
#endif
#include "ExitGroup.h"
#include "thread"

int64_t ExitGroup::Call(int64_t status, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
#ifdef EXIT_GROUP_DEBUG
    std::cout << "exit_group(" << status << ") pid " << process->getpid() << " tid " << process->gettid() << "\n";
#endif
    process->SetExitCode(status);
    scheduler->user_exit(status);
    process->SetKilled();
    process->CallAbortAll();
    params.DoContextSwitch(true);
    return 0;
}