//
// Created by sigsegv on 6/29/21.
//

#ifndef JEOKERNEL_TIME_H
#define JEOKERNEL_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef uint64_t time_t;

    time_t time(time_t *tloc);

    char *ctime(const time_t *timep);

#ifdef __cplusplus
}
#endif

#endif //JEOKERNEL_TIME_H
