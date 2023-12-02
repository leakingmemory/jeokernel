//
// Created by sigsegv on 10/9/22.
//

#include "Getpid.h"
#include <exec/procthread.h>

int64_t Getpid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    return params.CurrentThread()->getpid();
}