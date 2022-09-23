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
    return desc.ioctl(cmd, arg);
}