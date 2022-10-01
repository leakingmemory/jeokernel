//
// Created by sigsegv on 9/22/22.
//

#include "Gettimeofday.h"
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <time.h>

void Gettimeofday::DoGettimeofday(timeval *tv, timezone *tz) {
    clock_gettimeofday(tv, tz);
}

int64_t Gettimeofday::Call(int64_t uptr_tv, int64_t uptr_tz, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    if (uptr_tv != 0) {
        auto result = process->resolve_read(uptr_tv, sizeof(timeval), false, [scheduler, current_task] (intptr_t result) {
            scheduler->when_not_running(*current_task, [current_task, result]() {
                current_task->get_cpu_state().rax = (uint64_t) result;
                current_task->set_blocked(false);
            });
        }, [this, process, uptr_tv, uptr_tz](bool success, bool async, std::function<void (intptr_t)> asyncReturn) {
            if (!success || !process->resolve_write(uptr_tv, sizeof(timeval))) {
                return resolve_return_value::Return(-EFAULT);
            }
            if (uptr_tz != 0) {
                process->resolve_read(uptr_tz, sizeof(timezone), async, [asyncReturn] (intptr_t result) mutable {
                    asyncReturn(result);
                }, [this, process, uptr_tv, uptr_tz] (bool success, bool, const std::function<void (intptr_t)> &) {
                    if (!success || !process->resolve_write(uptr_tz, sizeof(timezone))) {
                        return resolve_return_value::Return(-EFAULT);
                    }
                    UserMemory tv{*process, (uintptr_t) uptr_tv, sizeof(timezone), true};
                    UserMemory tz{*process, (uintptr_t) uptr_tz, sizeof(timezone), true};
                    if (!tv || !tz) {
                        return resolve_return_value::Return(-EFAULT);
                    }
                    DoGettimeofday((timeval *) tv.Pointer(), (timezone *) tz.Pointer());
                    return resolve_return_value::Return(0);
                });
            } else {
                UserMemory tv{*process, (uintptr_t) uptr_tv, sizeof(timezone), true};
                if (!tv) {
                    return resolve_return_value::Return(-EFAULT);
                }
                DoGettimeofday((timeval *) tv.Pointer(), nullptr);
                return resolve_return_value::Return(0);
            }
        });
        if (result.async) {
            current_task->set_blocked(true);
            params.DoContextSwitch(true);
            return 0;
        } else {
            return result.result;
        }
    } else if (uptr_tz != 0) {
        auto result = process->resolve_read(uptr_tz, sizeof(timezone), false, [scheduler, current_task] (intptr_t result) {
            scheduler->when_not_running(*current_task, [current_task, result]() {
                current_task->get_cpu_state().rax = (uint64_t) result;
                current_task->set_blocked(false);
            });
        }, [this, process, uptr_tz] (bool success, bool, const std::function<void (intptr_t)> &) {
            if (!success || !process->resolve_write(uptr_tz, sizeof(timezone))) {
                return resolve_return_value::Return(-EFAULT);
            }
            UserMemory tz{*process, (uintptr_t) uptr_tz, sizeof(timezone), true};
            if (!tz) {
                return resolve_return_value::Return(-EFAULT);
            }
            DoGettimeofday(nullptr, (timezone *) tz.Pointer());
            return resolve_return_value::Return(0);
        });
        if (result.async) {
            current_task->set_blocked(true);
            params.DoContextSwitch(true);
            return 0;
        } else {
            return result.result;
        }
    }
    return 0;
}