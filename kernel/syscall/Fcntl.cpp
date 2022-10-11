//
// Created by sigsegv on 10/11/22.
//

#include "Fcntl.h"
#include <exec/procthread.h>
#include <errno.h>
#include <iostream>

int64_t Fcntl::Call(int64_t fd, int64_t cmd, int64_t arg, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto fdesc = process->get_file_descriptor(fd);
    if (!fdesc.Valid()) {
        return -EBADF;
    }
    std::cout << "fcntl(fd, 0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}
