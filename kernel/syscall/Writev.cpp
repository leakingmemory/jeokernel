//
// Created by sigsegv on 7/25/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include <errno.h>
#include "Writev.h"

int64_t Writev::Call(int64_t fd, int64_t user_iov_ptr, int64_t iovcnt, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto desc = process->get_file_descriptor(fd);
    if (!desc.Valid()) {
        return -EBADF;
    }
    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    desc.writev(process, user_iov_ptr, iovcnt, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
    return 0;
}