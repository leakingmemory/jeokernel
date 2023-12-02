//
// Created by sigsegv on 10/31/22.
//

#include "Setpgid.h"
#include <exec/procthread.h>

int64_t Setpgid::Call(int64_t pid, int64_t pgid, int64_t, int64_t, SyscallAdditionalParams &params) {
    return params.CurrentThread()->setpgid(pid, pgid);
}