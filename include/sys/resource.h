//
// Created by sigsegv on 7/29/22.
//

#ifndef JEOKERNEL_RESOURCE_H
#define JEOKERNEL_RESOURCE_H

#include <cstdint>

typedef uint64_t rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

#define RLIMIT_CPU          0
#define RLIMIT_FSIZE        1
#define RLIMIT_DATA         2
#define RLIMIT_STACK        3
#define RLIMIT_CORE         4
#define RLIMIT_RSS          5
#define RLIMIT_NPROC        6
#define RLIMIT_NOFILE       7
#define RLIMIT_MEMLOCK      8
#define RLIMIT_AS           9
#define RLIMIT_LOCKS        10
#define RLIMIT_SIGPENDING   11
#define RLIMIT_MSGQUEUE     12
#define RLIMIT_NICE         13
#define RLIMIT_RTPRIO       14
#define RLIMIT_RTTIME       15

#endif //JEOKERNEL_RESOURCE_H
