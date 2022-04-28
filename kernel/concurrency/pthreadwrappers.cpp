//
// Created by sigsegv on 4/28/22.
//

#include <pthread.h>

extern "C" {

    int pthread_mutex_lock(pthread_mutex_t *mutex) {
        mutex->mtx.lock();
        return 0;
    }

    int pthread_mutex_unlock(pthread_mutex_t *mutex) {
        mutex->mtx.unlock();
        return 0;
    }

    int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
        mutex->mtx.unlock();
        cond->sema.acquire();
        mutex->mtx.lock();
        return 0;
    }
    int pthread_cond_signal(pthread_cond_t *cond) {
        cond->sema.release();
        return 0;
    }
}

