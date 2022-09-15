//
// Created by sigsegv on 9/15/22.
//

#include "Pread64.h"
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>

int64_t Pread64::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t pos, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto desc = process->get_file_descriptor(fd);
    if (!desc.Valid()) {
        return -EBADF;
    }
    current_task->set_blocked(true);
    additionalParams.DoContextSwitch(true);
    process->resolve_read(uptr_buf, count, [this, process, scheduler, current_task, desc, uptr_buf, count, pos] (bool success) mutable {
        if (success) {
            success = process->resolve_write(uptr_buf, count);
        }
        if (success) {
            Queue([process, scheduler, current_task, uptr_buf, count, pos, desc] () mutable {
                UserMemory umem{*process, (uintptr_t) uptr_buf, (uintptr_t) count, true};
                if (!umem) {
                    scheduler->when_not_running(*current_task, [current_task] () {
                        current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                        current_task->set_blocked(false);
                    });
                    return;
                }
                auto result = desc.read(umem.Pointer(), count, pos);
                scheduler->when_not_running(*current_task, [current_task, result]() {
                    current_task->get_cpu_state().rax = (uint64_t) result;
                    current_task->set_blocked(false);
                });
            });
        } else {
            scheduler->when_not_running(*current_task, [current_task] () {
                current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                current_task->set_blocked(false);
            });
        }
    });
    return 0;
}