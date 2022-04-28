//
// Created by sigsegv on 4/28/22.
//

#ifndef JEOKERNEL_PTHREAD_H
#define JEOKERNEL_PTHREAD_H

#include <concurrency/mutex.h>
#include <concurrency/raw_semaphore.h>
#include <sched.h>

typedef struct {

} pthread_key_t;

typedef struct {
    uint32_t called;
    uint32_t finished;
} pthread_once_t;

#define PTHREAD_ONCE_INIT {0, 0}

typedef struct {
#ifdef __cplusplus
    std::mutex mtx;
#else
    jeokernel_mutex_fields mtx;
#endif
} pthread_mutex_t;

#ifdef __cplusplus
#define PTHREAD_MUTEX_INITIALIZER {.mtx = std::mutex()}
#else
#define PTHREAD_MUTEX_INITIALIZER {.mtx = jeokernel_mutex_field_initvalues}
#endif

typedef struct {
#ifdef __cplusplus
    raw_semaphore sema;
#else
    jeokernel_raw_semaphore_fields sema;
#endif
} pthread_cond_t;

#ifdef __cplusplus
#define PTHREAD_COND_INITIALIZER {.sema = raw_semaphore(-1)}
#else
#define PTHREAD_COND_INITIALIZER {.sema = jeokernel_raw_semaphore_field_initvalues}
#endif

#ifdef __cplusplus
extern "C" {
#endif

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_setspecific(pthread_key_t key, const void *value);
void *pthread_getspecific(pthread_key_t key);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);

#ifdef __cplusplus
}
#endif

#endif //JEOKERNEL_PTHREAD_H
