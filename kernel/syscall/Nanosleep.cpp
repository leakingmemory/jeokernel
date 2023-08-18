//
// Created by sigsegv on 8/17/23.
//

#include <errno.h>
#include <time.h>
#include <core/scheduler.h>
#include <core/nanotime.h>
#include "Nanosleep.h"
#include "SyscallCtx.h"

void Nanosleep::DoNanosleep(std::shared_ptr<SyscallCtx> ctx, class task &task, const timespec * const request, timespec * const remaining) {
    auto *scheduler = get_scheduler();
    auto nano_start = get_nanotime_ref();
    typeof(nano_start) nanos = request->tv_sec;
    nanos *= 1000000000ULL;
    nanos += request->tv_nsec;
    typeof(nano_start) nano_request = nano_start + nanos;
    scheduler->when_not_running(task, [ctx, &task, remaining, nano_request] () {
        if (remaining != nullptr) {
            remaining->tv_nsec = 0;
            remaining->tv_sec = 0;
        }
        auto nanos_ref = get_nanotime_ref();
        if (nanos_ref >= nano_request) {
            task.set_blocked(false);
        } else {
            task.add_event_handler(new task_nanos_handler(task.get_id(), nano_request));
        }
    });
}

int64_t Nanosleep::Call(int64_t uptr_requested, int64_t uptr_remaining, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto ctx = std::make_shared<SyscallCtx>(params);
    auto &task = get_scheduler()->get_current_task();
    if (uptr_requested == 0) {
        return -EINVAL;
    }
    return ctx->Read(uptr_requested, sizeof(timespec), [ctx, &task, uptr_remaining] (void *ptr_requested) {
        auto *requested = (timespec *) ptr_requested;
        if (uptr_remaining != 0) {
            return ctx->NestedWrite(uptr_remaining, sizeof(timespec), [ctx, &task, requested] (void *ptr_remaining) {
                auto *remaining = (timespec *) ptr_remaining;
                DoNanosleep(ctx, task, requested, remaining);
                return resolve_return_value::AsyncReturn();
            });
        }
        DoNanosleep(ctx, task, requested, nullptr);
        return resolve_return_value::AsyncReturn();
    });
}
