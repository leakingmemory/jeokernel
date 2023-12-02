//
// Created by sigsegv on 9/14/22.
//

#include "Close.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Close::Call(int64_t fd, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *process = params.CurrentThread();
    return process->close_file_descriptor((int) fd) ? 0 : -EBADF;
}
