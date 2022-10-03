//
// Created by sigsegv on 9/23/22.
//

#include "Ioctl.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Ioctl::Call(int64_t fd, int64_t cmd, int64_t arg, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto desc = process->get_file_descriptor(fd);
    if (!desc.Valid()) {
        return -EBADF;
    }
    auto result = desc.ioctl(cmd, arg, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
    if (result.async) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        return 0;
    } else  {
        return result.result;
    }
}