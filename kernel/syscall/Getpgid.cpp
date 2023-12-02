//
// Created by sigsegv on 8/10/23.
//

#include <exec/procthread.h>
#include "Getpgid.h"

int64_t Getpgid::Call(int64_t pid, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    return params.CurrentThread()->getpgid((pid_t) pid);
}