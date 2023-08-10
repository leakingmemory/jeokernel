//
// Created by sigsegv on 8/11/23.
//

#ifndef JEOKERNEL_SYSCLONEIMPL_H
#define JEOKERNEL_SYSCLONEIMPL_H

#include <cstdint>

#define CLONE_CHILD_SETTID          0x1000000
#define CLONE_CHILD_CLEARTID        0x0200000
#define CLONE_CHILD_EXIT_SIGNALMASK 0x00000FF

constexpr unsigned int CloneSupportedFlags = (
        CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | CLONE_CHILD_EXIT_SIGNALMASK
);

class SyscallAdditionalParams;

class SysCloneImpl {
public:
    static int DoClone(int64_t flags, int64_t new_stackp, int64_t uptr_parent_tidptr, int64_t uptr_child_tidptr, int64_t tls, SyscallAdditionalParams &params);
};


#endif //JEOKERNEL_SYSCLONEIMPL_H
