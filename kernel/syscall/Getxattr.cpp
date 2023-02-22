//
// Created by sigsegv on 2/22/23.
//

#include <exec/procthread.h>
#include <errno.h>
#include "Getxattr.h"
#include "SyscallCtx.h"

int64_t Getxattr::Call(int64_t uptr_pathname, int64_t uptr_attrname, int64_t uptr_value, int64_t size, SyscallAdditionalParams &params) {
    auto flags = params.Param5();
    return -EOPNOTSUPP;
}
