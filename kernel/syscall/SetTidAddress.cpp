//
// Created by sigsegv on 7/17/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include "SetTidAddress.h"

int64_t SetTidAddress::Call(int64_t tidptr_i, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto tidptr = (uintptr_t) tidptr_i;

    auto *process = params.CurrentThread();

    process->SetTidAddress(tidptr);
    return process->getpid();
}