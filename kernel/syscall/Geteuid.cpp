//
// Created by sigsegv on 7/11/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include "Geteuid.h"

int64_t Geteuid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    return params.CurrentThread()->geteuid();
}