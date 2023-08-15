//
// Created by sigsegv on 8/11/23.
//

#ifndef JEOKERNEL_SYSCLONEIMPL_H
#define JEOKERNEL_SYSCLONEIMPL_H

#include <cstdint>
#include <memory>

#define CLONE_VM                    0x0000100
#define CLONE_FS                    0x0000200
#define CLONE_FILES                 0x0000400
#define CLONE_SIGHAND               0x0000800
#define CLONE_THREAD                0x0010000
#define CLONE_SYSVSEM               0x0040000
#define CLONE_SETTLS                0x0080000
#define CLONE_PARENT_SETTID         0x0100000
#define CLONE_CHILD_CLEARTID        0x0200000
#define CLONE_DETACHED              0x0400000
#define CLONE_CHILD_SETTID          0x1000000

#define CLONE_CHILD_EXIT_SIGNALMASK 0x00000FF

constexpr unsigned int CloneSupportedFlags = (
        CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | CLONE_CHILD_EXIT_SIGNALMASK |
        CLONE_DETACHED | CLONE_PARENT_SETTID | CLONE_SETTLS | CLONE_SYSVSEM |
        CLONE_SIGHAND | CLONE_FILES | CLONE_FS | CLONE_VM | CLONE_THREAD
);

constexpr unsigned int ThreadFlags = (CLONE_SIGHAND | CLONE_FILES | CLONE_FS | CLONE_VM | CLONE_THREAD);

class SyscallAdditionalParams;
class callctx_async;
class Process;
class TaskCloner;

class SysCloneImpl {
public:
    static void DoClone(std::shared_ptr<callctx_async> async, std::shared_ptr<Process> clonedProcess, std::shared_ptr<TaskCloner> taskCloner);
    static int DoClone(int64_t flags, int64_t new_stackp, int64_t uptr_parent_tidptr, int64_t uptr_child_tidptr, int64_t tls, SyscallAdditionalParams &params);
};


#endif //JEOKERNEL_SYSCLONEIMPL_H
