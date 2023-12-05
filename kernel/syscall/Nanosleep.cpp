//
// Created by sigsegv on 8/17/23.
//

//#define DEBUG_NANOSLEEP

#include <errno.h>
#include <time.h>
#include <core/scheduler.h>
#include <core/nanotime.h>
#include <exec/procthread.h>
#include "Nanosleep.h"
#include "SyscallCtx.h"
#ifdef DEBUG_NANOSLEEP
#include <iostream>
#endif

void Nanosleep::DoNanosleep(std::shared_ptr<SyscallCtx> ctx, class task &task, const timespec * const request, timespec * const remaining) {
    auto *scheduler = get_scheduler();
    auto nano_start = get_nanotime_ref();
    typeof(nano_start) nanos = request->tv_sec;
    nanos *= 1000000000ULL;
    nanos += request->tv_nsec;
    typeof(nano_start) nano_request = nano_start + nanos;
#ifdef DEBUG_NANOSLEEP
    std::cout << "Nanosleep req " << nano_request << "\n";
#endif
    scheduler->when_not_running(task, [ctx, scheduler, &task, remaining, nano_request] () {
        if (remaining != nullptr) {
            remaining->tv_nsec = 0;
            remaining->tv_sec = 0;
        }
        auto nanos_ref = get_nanotime_ref();
        if (nanos_ref >= nano_request) {
            task.set_blocked("nanslpre", false);
#ifdef DEBUG_NANOSLEEP
            scheduler->when_out_of_lock([] () {
                std::cout << "Nanosleep short\n";
            });
#endif
        } else {
            task.add_event_handler(new task_nanos_handler(task.get_id(), nano_request));
            auto *pt = task.get_resource<ProcThread>();
            auto task_id = task.get_id();
            pt->AborterFunc([ctx, scheduler, remaining, nano_request, task_id] () {
                scheduler->all_tasks([ctx, scheduler, remaining, nano_request, task_id] (class task &task) {
                    if (task_id == task.get_id()) {
                        auto nanosHandler = task.get_event_handler<task_nanos_handler>();
                        if (nanosHandler != nullptr) {
                            task.remove_event_handler(nanosHandler);
                            delete nanosHandler;
                            scheduler->when_out_of_lock([ctx, remaining, nano_request] () {
#ifdef DEBUG_NANOSLEEP
                                std::cout << "Nanosleep aborted\n";
#endif
                                if (remaining != nullptr) {
                                    auto nanos_ref = get_nanotime_ref();
                                    if (nanos_ref < nano_request) {
                                        auto nanos_remaining = nano_request - nanos_ref;
                                        auto seconds_remaining = nanos_remaining / 1000000000;
                                        nanos_remaining = nanos_remaining % 1000000000;
                                        remaining->tv_sec = (time_t) seconds_remaining;
                                        remaining->tv_nsec = (long) nanos_remaining;
                                    }
                                }
                                ctx->ReturnWhenNotRunning(-EINTR);
                            });
                        }
                    }
                });
            });
            if (pt->HasPendingSignalOrKill()) {
                pt->CallAbort();
            }
        }
    });
}

int64_t Nanosleep::Call(int64_t uptr_requested, int64_t uptr_remaining, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto ctx = std::make_shared<SyscallCtx>(params, "Nanoslee");
    auto *task = params.CurrentTask();
    if (uptr_requested == 0) {
        return -EINVAL;
    }
    return ctx->Read(uptr_requested, sizeof(timespec), [ctx, task, uptr_remaining] (void *ptr_requested) {
        auto *requested = (timespec *) ptr_requested;
        if (uptr_remaining != 0) {
            return ctx->NestedWrite(uptr_remaining, sizeof(timespec), [ctx, task, requested] (void *ptr_remaining) {
                auto *remaining = (timespec *) ptr_remaining;
                DoNanosleep(ctx, *task, requested, remaining);
                return resolve_return_value::AsyncReturn();
            });
        }
        DoNanosleep(ctx, *task, requested, nullptr);
        return resolve_return_value::AsyncReturn();
    });
}
