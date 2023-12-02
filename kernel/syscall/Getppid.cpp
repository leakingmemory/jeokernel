//
// Created by sigsegv on 10/10/22.
//

#include "Getppid.h"
#include <exec/procthread.h>

int64_t Getppid::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    return params.CurrentThread()->getppid();
}