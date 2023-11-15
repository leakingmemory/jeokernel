//
// Created by sigsegv on 10/10/22.
//

#include "Dup.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Dup::Call(int64_t oldfd, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto old = process->get_file_descriptor(oldfd);
    if (!old) {
        return -EBADF;
    }
    auto newf = process->create_file_descriptor(old->get_open_flags(), old->GetHandler()->clone());
    if (!newf) {
        return -EMFILE;
    }
    return newf->FD();
}