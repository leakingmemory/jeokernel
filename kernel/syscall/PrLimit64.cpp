//
// Created by sigsegv on 8/1/22.
//

#include <iostream>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include "PrLimit64.h"

//#define PRLIM64_DEBUG

int64_t PrLimit64::Call(int64_t pid, int64_t resource, int64_t uptr_newlim, int64_t uptr_oldlim, SyscallAdditionalParams &params) {
    auto *scheduler = params.Scheduler();
    task *current_task = params.CurrentTask();
    auto *process = params.CurrentThread();
#ifdef PRLIM64_DEBUG
    std::cout << "prlimit64(" << pid << ", " << resource << ", " << std::hex << uptr_newlim << ", " << uptr_oldlim << std::dec << ")\n";
#endif
    if (pid != 0 && pid != process->getpid()) {
        return -EPERM;
    }
    if (uptr_newlim != 0) {
        auto result = process->resolve_read(uptr_newlim, sizeof(rlimit), false, [scheduler, current_task] (intptr_t result) {
            scheduler->when_not_running(*current_task, [current_task, result] () {
                current_task->get_cpu_state().rax = (uint64_t) result;
                current_task->set_blocked("prlimret", false);
            });
        }, [scheduler, current_task, process, uptr_newlim, uptr_oldlim, resource] (bool success, bool async, std::function<void (intptr_t)> asyncFunc) {
            if (!success) {
                return resolve_return_value::Return(-EFAULT);
            }
            if (uptr_oldlim != 0) {
                auto result = process->resolve_read(uptr_oldlim, sizeof(rlimit), async, asyncFunc, [process, uptr_newlim, uptr_oldlim, resource] (bool success, bool, const std::function<void (intptr_t)> &) {
                    if (success) {
                        if (!process->resolve_write(uptr_oldlim, sizeof(rlimit))) {
                            success = false;
                        }
                    }
                    if (!success) {
                        return resolve_return_value::Return(-EFAULT);
                    }
                    UserMemory uptr_old_mem{*process, (uintptr_t) uptr_oldlim, (uintptr_t) sizeof(rlimit), true};
                    UserMemory uptr_new_mem{*process, (uintptr_t) uptr_newlim, (uintptr_t) sizeof(rlimit)};
                    rlimit tmp{};
                    auto result = process->getrlimit(resource, tmp);
                    if (result == 0) {
                        result = process->setrlimit(resource, *((rlimit *) uptr_new_mem.Pointer()));
                        if (result == 0) {
                            memcpy(uptr_old_mem.Pointer(), &tmp, sizeof(tmp));
                        }
                    }
                    return resolve_return_value::Return(result);
                });
                if (result.async) {
                    return resolve_return_value::AsyncReturn();
                } else {
                    return resolve_return_value::Return(result.result);
                }
            } else {
                UserMemory uptr_new_mem{*process, (uintptr_t) uptr_newlim, (uintptr_t) sizeof(rlimit)};
                auto result = process->setrlimit(resource, *((rlimit *) uptr_new_mem.Pointer()));
                return resolve_return_value::Return(result);
            }
        });
        if (result.async) {
            current_task->set_blocked("prlim64", true);
            params.DoContextSwitch(true);
            return 0;
        } else {
            return result.result;
        }
    } else {
        if (uptr_oldlim != 0) {
            auto result = process->resolve_read(uptr_oldlim, sizeof(rlimit), false, [scheduler, current_task] (intptr_t result) {
                scheduler->when_not_running(*current_task, [current_task, result] () {
                    current_task->get_cpu_state().rax = (uint64_t) result;
                    current_task->set_blocked("prlimret", false);
                });
            }, [scheduler, current_task, process, uptr_oldlim, resource] (bool success, bool, const std::function<void (intptr_t)> &) {
                if (success) {
                    if (!process->resolve_write(uptr_oldlim, sizeof(rlimit))) {
                        success = false;
                    }
                }
                if (!success) {
                    return resolve_return_value::Return(-EFAULT);
                }
                UserMemory uptr_old_mem{*process, (uintptr_t) uptr_oldlim, (uintptr_t) sizeof(rlimit), true};
                auto result = process->getrlimit(resource, *((rlimit *) uptr_old_mem.Pointer()));
                return resolve_return_value::Return(result);
            });
            if (result.async) {
                current_task->set_blocked("2prlim64", true);
                params.DoContextSwitch(true);
                return 0;
            } else {
                return result.result;
            }
        }
    }
    return 0;
}