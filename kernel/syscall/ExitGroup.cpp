//
// Created by sigsegv on 8/11/22.
//

#include <exec/procthread.h>
#include "ExitGroup.h"
#include "thread"

int64_t ExitGroup::Call(int64_t status, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = current_task->get_resource<ProcThread>();
    auto pid = process->getpid();
    scheduler->all_tasks([pid] (task &t) {
        auto *process = t.get_resource<ProcThread>();
        if (process != nullptr && pid == process->getpid()) {
            t.set_end(true);
        }
    });
    params.DoContextSwitch(true);
    return 0;
}