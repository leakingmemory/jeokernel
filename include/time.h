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

    struct timespec {
        time_t tv_sec;
        long int tv_nsec;
    };

    int clock_gettime(int which, struct timespec *);

    time_t time(time_t *tloc);

    char *ctime(const time_t *timep);

#ifdef __cplusplus
}
#endif

#endif //JEOKERNEL_TIME_H
