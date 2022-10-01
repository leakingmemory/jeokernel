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
    auto result = process->resolve_read_nullterm(uptr_path, false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    }, [process, uptr_path, uptr_buf, bufsize](bool success, bool async, size_t slen, std::function<void (intptr_t)> asyncFunc) {
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        std::string path{};
        {
            UserMemory path_mem{*process, (uintptr_t) uptr_path, slen};
            path.append((const char *) path_mem.Pointer(), slen);
        }
        auto result = process->resolve_read(uptr_buf, bufsize, async, asyncFunc, [process, path, uptr_buf, bufsize] (bool success, bool, const std::function<void (intptr_t)> &) {
            if (success) {
                if (!process->resolve_write(uptr_buf, bufsize)) {
                    success = false;
                }
            }
            if (!success) {
                return resolve_return_value::Return(-EFAULT);
            }
            UserMemory buf_mem{*process, (uintptr_t) uptr_buf, (uintptr_t) bufsize, true};
            char *buf = (char *) buf_mem.Pointer();

            // TODO - implement symlinks

            return resolve_return_value::Return(-ENOENT);
        });
        if (result.async) {
            return resolve_return_value::AsyncReturn();
        } else {
            return resolve_return_value::Return(result.result);
        }
    });
    if (result.async) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}
