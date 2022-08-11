//
// Created by sigsegv on 8/2/22.
//

#include <iostream>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include "Readlink.h"

int64_t Readlink::Call(int64_t uptr_path, int64_t uptr_buf, int64_t bufsize, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    current_task->set_blocked(true);
    params.DoContextSwitch(true);
    process->resolve_read_nullterm(uptr_path, [scheduler, current_task, process, uptr_path, uptr_buf, bufsize](bool success, size_t slen) {
        if (!success) {
            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
            return;
        }
        std::string path{};
        {
            UserMemory path_mem{*process, (uintptr_t) uptr_path, slen};
            path.append((const char *) path_mem.Pointer(), slen);
        }
        process->resolve_read(uptr_buf, bufsize, [scheduler, current_task, process, path, uptr_buf, bufsize] (bool success) {
            if (success) {
                if (!process->resolve_write(uptr_buf, bufsize)) {
                    success = false;
                }
            }
            if (!success) {
                scheduler->when_not_running(*current_task, [current_task] () {
                    current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                    current_task->set_blocked(false);
                });
                return;
            }
            UserMemory buf_mem{*process, (uintptr_t) uptr_buf, (uintptr_t) bufsize, true};
            char *buf = (char *) buf_mem.Pointer();

            // TODO - implement symlinks

            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -ENOENT;
                current_task->set_blocked(false);
            });
        });
    });
    return 0;
}
