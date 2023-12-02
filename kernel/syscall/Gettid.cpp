//
// Created by sigsegv on 8/11/23.
//

#include "Gettid.h"
#include <exec/procthread.h>

int64_t Gettid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    return params.CurrentThread()->getpid();
}