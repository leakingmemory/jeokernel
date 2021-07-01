//
// Created by sigsegv on 7/1/21.
//

#ifndef JEOKERNEL_SIGNAL_H
#define JEOKERNEL_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stub, just to have acpica compile
 */

typedef void (*sighandler_t)(int);

#define SIG_IGN ((sighandler_t) (void *) 0)

sighandler_t signal(int signal, sighandler_t handler);

#define SIGINT 2

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_SIGNAL_H
