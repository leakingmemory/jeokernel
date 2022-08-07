//
// Created by sigsegv on 8/7/22.
//

#include <time.h>
#include <core/scheduler.h>

static int64_t clock_offset_ticks = 0;

extern "C" int clock_gettime(int which, struct timespec *tv) {
    auto ticks = get_ticks();
    ticks += clock_offset_ticks;
    tv->tv_sec = ticks / 100;
    tv->tv_nsec = ticks % 100;
    tv->tv_nsec *= (1000000000L/100L);
    return 0;
}
