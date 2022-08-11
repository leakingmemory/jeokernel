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
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
#ifdef PRLIM64_DEBUG
    std::cout << "prlimit64(" << pid << ", " << resource << ", " << std::hex << uptr_newlim << ", " << uptr_oldlim << std::dec << ")\n";
#endif
    if (pid != 0 && pid != process->getpid()) {
        return -EPERM;
    }
    if (uptr_newlim != 0) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        process->resolve_read(uptr_newlim, sizeof(rlimit), [scheduler, current_task, process, uptr_newlim, uptr_oldlim, resource] (bool success) {
            if (!success) {
                scheduler->when_not_running(*current_task, [current_task] () {
                    current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                    current_task->set_blocked(false);
                });
                return;
            }
            if (uptr_oldlim != 0) {
                process->resolve_read(uptr_oldlim, sizeof(rlimit), [scheduler, current_task, process, uptr_newlim, uptr_oldlim, resource] (bool success) {
                    if (success) {
                        if (!process->resolve_write(uptr_oldlim, sizeof(rlimit))) {
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
                    scheduler->when_not_running(*current_task, [current_task, result] () {
                        current_task->get_cpu_state().rax = (uint64_t) result;
                        current_task->set_blocked(false);
                    });
                });
            } else {
                UserMemory uptr_new_mem{*process, (uintptr_t) uptr_newlim, (uintptr_t) sizeof(rlimit)};
                auto result = process->setrlimit(resource, *((rlimit *) uptr_new_mem.Pointer()));
                scheduler->when_not_running(*current_task, [current_task, result] () {
                    current_task->get_cpu_state().rax = (uint64_t) result;
                    current_task->set_blocked(false);
                });
            }
        });
    } else {
        if (uptr_oldlim != 0) {
            current_task->set_blocked(true);
            params.DoContextSwitch(true);
            process->resolve_read(uptr_oldlim, sizeof(rlimit), [scheduler, current_task, process, uptr_oldlim, resource] (bool success) {
                if (success) {
                    if (!process->resolve_write(uptr_oldlim, sizeof(rlimit))) {
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
                UserMemory uptr_old_mem{*process, (uintptr_t) uptr_oldlim, (uintptr_t) sizeof(rlimit), true};
                auto result = process->getrlimit(resource, *((rlimit *) uptr_old_mem.Pointer()));
                scheduler->when_not_running(*current_task, [current_task, result] () {
                    current_task->get_cpu_state().rax = (uint64_t) result;
                    current_task->set_blocked(false);
                });
            });
        } else {
            return 0;
        }
    }
    return 0;
}