//
// Created by sigsegv on 9/14/22.
//

#include "Read.h"
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>
#include <exec/usermem.h>

int64_t Read::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto desc = process->get_file_descriptor(fd);
    if (!desc.Valid()) {
        return -EBADF;
    }
    auto result = process->resolve_read(uptr_buf, count, false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    }, [this, process, desc, uptr_buf, count] (bool success, bool, std::function<void (intptr_t)> asyncFunc) mutable {
        if (success) {
            success = process->resolve_write(uptr_buf, count);
        }
        if (success) {
            Queue([process, asyncFunc, uptr_buf, count, desc] () mutable {
                UserMemory umem{*process, (uintptr_t) uptr_buf, (uintptr_t) count, true};
                if (!umem) {
                    asyncFunc(-EFAULT);
                    return;
                }
                auto result = desc.read(umem.Pointer(), count);
                asyncFunc(result);
            });
            return resolve_return_value::AsyncReturn();
        } else {
            return resolve_return_value::Return(-EFAULT);
        }
    });
    if (result.async) {
        current_task->set_blocked(true);
        additionalParams.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}
