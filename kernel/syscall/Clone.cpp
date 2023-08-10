//
// Created by sigsegv on 11/4/22.
//

#include "Clone.h"

int64_t Clone::Call(int64_t flags, int64_t new_stackp, int64_t uptr_parent_tidptr, int64_t uptr_child_tidptr, SyscallAdditionalParams &params) {
    auto tls = params.Param5();
    return DoClone(flags, new_stackp, uptr_parent_tidptr, uptr_child_tidptr, tls, params);
}