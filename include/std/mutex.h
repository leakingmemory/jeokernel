//
// Created by sigsegv on 06.05.2021.
//

#ifndef JEOKERNEL_MUTEX_H
#define JEOKERNEL_MUTEX_H

#include <concurrency/mutex.h>

namespace std {
    template <class Mutex> class lock_guard {
    private:
        Mutex &mtx;
    public:
        lock_guard(const lock_guard &) = delete;
        lock_guard(lock_guard &&) = delete;
        lock_guard() = delete;
        lock_guard &operator =(const lock_guard &) = delete;
        lock_guard &operator =(lock_guard &&) = delete;

        lock_guard(Mutex &mutex) : mtx(mutex) {
            mtx.lock();
        }
        ~lock_guard() {
            mtx.unlock();
        }
    };
}

#endif //JEOKERNEL_MUTEX_H
