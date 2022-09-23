//
// Created by sigsegv on 6/29/21.
//

#ifndef JEOKERNEL_TIME_H
#define JEOKERNEL_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef int64_t time_t;
    typedef int64_t suseconds_t;

    struct timespec {
        time_t tv_sec;
        long int tv_nsec;
    };

    struct timeval {
        time_t tv_sec;
        suseconds_t tv_usec;
    };

    struct timezone {
        int tz_minuteswest;
        int tz_dsttime;
    };

    int clock_gettime(int which, struct timespec *);
    void clock_gettimeofday(struct timeval *tv, struct timezone *tz);

    time_t time(time_t *tloc);

    char *ctime(const time_t *timep);

#ifdef __cplusplus
}
#endif

#endif //JEOKERNEL_TIME_H
