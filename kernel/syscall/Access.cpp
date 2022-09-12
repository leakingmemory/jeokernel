//
// Created by sigsegv on 9/8/22.
//

#include "Access.h"
#include <exec/usermem.h>
#include <exec/procthread.h>
#include <kfs/kfiles.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>

int Access::DoAccess(ProcThread &proc, std::string filename, int mode) {
    constexpr int allAccess = F_OK | X_OK | W_OK | R_OK;
    if ((mode & allAccess) != mode) {
        return -EINVAL;
    }
    auto file = proc.ResolveFile(filename);
    if (!file) {
        return -ENOENT;
    }
    int m = file->Mode();
    if ((m & mode) == mode) {
        return 0;
    }
    mode = mode & ~m;
    m = m >> 3;
    {
        auto gid = proc.getgid();
        auto f_gid = file->Gid();
        if (gid == f_gid) {
            if ((m & mode) == mode) {
                return 0;
            }
            mode = mode & ~m;
        }
    }
    m = m >> 3;
    {
        auto uid = proc.getuid();
        auto f_uid = file->Uid();
        if (uid == f_uid || uid == 0) {
            if ((m & mode) == mode) {
                return 0;
            }
        }
    }
    return -EACCES;
}

int64_t Access::Call(int64_t uptr_filename, int64_t mode, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    process->resolve_read_nullterm(uptr_filename, [this, scheduler, current_task, process, uptr_filename, mode] (bool success, size_t slen) {
        if (!success) {
            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
            return;
        }
        std::string filename{};
        {
            UserMemory filename_mem{*process, (uintptr_t) uptr_filename, slen};
            filename.append((const char *) filename_mem.Pointer(), slen);
        }
        Queue([this, filename, mode, process, scheduler, current_task] () {
            auto res = DoAccess(*process, filename, mode);
            std::cout << "access(" << filename << ", " << std::hex << mode << std::dec << ") => " << res << "\n";
            scheduler->when_not_running(*current_task, [current_task, res] () {
                current_task->get_cpu_state().rax = (uint64_t) res;
                current_task->set_blocked(false);
            });
        });
    });
    return 0;
}