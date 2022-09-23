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
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        process->resolve_read(uptr_tv, sizeof(timeval), [this, process, scheduler, current_task, uptr_tv, uptr_tz](bool success) {
            if (!success || !process->resolve_write(uptr_tv, sizeof(timeval))) {
                scheduler->when_not_running(*current_task, [current_task]() {
                    current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                    current_task->set_blocked(false);
                });
                return;
            }
            if (uptr_tz != 0) {
                process->resolve_read(uptr_tz, sizeof(timezone), [this, process, scheduler, current_task, uptr_tv, uptr_tz] (bool success) {
                    if (!success || !process->resolve_write(uptr_tz, sizeof(timezone))) {
                        scheduler->when_not_running(*current_task, [current_task]() {
                            current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                            current_task->set_blocked(false);
                        });
                        return;
                    }
                    UserMemory tv{*process, (uintptr_t) uptr_tv, sizeof(timezone), true};
                    UserMemory tz{*process, (uintptr_t) uptr_tz, sizeof(timezone), true};
                    if (!tv || !tz) {
                        scheduler->when_not_running(*current_task, [current_task]() {
                            current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                            current_task->set_blocked(false);
                        });
                        return;
                    }
                    DoGettimeofday((timeval *) tv.Pointer(), (timezone *) tz.Pointer());
                    scheduler->when_not_running(*current_task, [current_task]() {
                        current_task->get_cpu_state().rax = (uint64_t) 0;
                        current_task->set_blocked(false);
                    });
                });
            } else {
                UserMemory tv{*process, (uintptr_t) uptr_tv, sizeof(timezone), true};
                if (!tv) {
                    scheduler->when_not_running(*current_task, [current_task]() {
                        current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                        current_task->set_blocked(false);
                    });
                    return;
                }
                DoGettimeofday((timeval *) tv.Pointer(), nullptr);
                scheduler->when_not_running(*current_task, [current_task]() {
                    current_task->get_cpu_state().rax = (uint64_t) 0;
                    current_task->set_blocked(false);
                });
            }
        });
    } else if (uptr_tz != 0) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        process->resolve_read(uptr_tz, sizeof(timezone), [this, process, scheduler, current_task, uptr_tz] (bool success) {
            if (!success || !process->resolve_write(uptr_tz, sizeof(timezone))) {
                scheduler->when_not_running(*current_task, [current_task]() {
                    current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                    current_task->set_blocked(false);
                });
                return;
            }
            UserMemory tz{*process, (uintptr_t) uptr_tz, sizeof(timezone), true};
            if (!tz) {
                scheduler->when_not_running(*current_task, [current_task]() {
                    current_task->get_cpu_state().rax = (uint64_t) -EFAULT;
                    current_task->set_blocked(false);
                });
                return;
            }
            DoGettimeofday(nullptr, (timezone *) tz.Pointer());
            scheduler->when_not_running(*current_task, [current_task]() {
                current_task->get_cpu_state().rax = (uint64_t) 0;
                current_task->set_blocked(false);
            });
        });
    }
    return 0;
}