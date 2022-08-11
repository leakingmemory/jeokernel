//
// Created by sigsegv on 7/12/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include <iostream>
#include <errno.h>
#include "Brk.h"

int64_t Brk::Call(int64_t addr, int64_t, int64_t, int64_t, SyscallAdditionalParams &) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    uintptr_t result{0};
    if (process->brk(addr, result)) {
        return (int64_t) result;
    } else {
        return -ENOMEM;
    }
}