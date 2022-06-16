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

    template <class Mutex> class unique_lock {
    private:
        Mutex *mtx;
    public:
        unique_lock(const unique_lock &) = delete;
        unique_lock(unique_lock &&mv) {
            unique_lock lock{};
            lock.swap(mv);
            swap(lock);
        };
        unique_lock() : mtx(nullptr) {
        }
        unique_lock &operator =(const unique_lock &) = delete;
        unique_lock &operator =(unique_lock &&mv) {
            unique_lock lock{};
            lock.swap(mv);
            swap(lock);
            return *this;
        }

        unique_lock(Mutex &mutex) : mtx(&mutex) {
            mtx->lock();
        }
        ~unique_lock() {
            release();
        }
        void swap( unique_lock& other ) noexcept {
            Mutex *mtx = this->mtx;
            this->mtx = other.mtx;
            other.mtx = mtx;
        }
        Mutex *release() noexcept {
            if (mtx != nullptr) {
                Mutex *mtx = this->mtx;
                this->mtx = nullptr;
                mtx->unlock();
                return mtx;
            }
            return nullptr;
        }
    };
}

#endif //JEOKERNEL_MUTEX_H
