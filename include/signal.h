//
// Created by sigsegv on 7/1/21.
//

#ifndef JEOKERNEL_SIGNAL_H
#define JEOKERNEL_SIGNAL_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    typedef uint32_t sigset_val_t;
    sigset_val_t val[1024 / (8*sizeof(sigset_val_t))];
} sigset_t;

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

typedef void (*sighandler_t)(int);

#define SIG_IGN ((sighandler_t) (void *) 0)

sighandler_t signal(int signal, sighandler_t handler);

#define SIGINT 2

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_SIGNAL_H
