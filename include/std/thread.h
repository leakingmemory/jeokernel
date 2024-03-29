//
// Created by sigsegv on 09.05.2021.
//

#ifndef JEOKERNEL_THREAD_H
#define JEOKERNEL_THREAD_H

#include <functional>
#include <concurrency/hw_spinlock.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include <klogger.h>
#include <core/scheduler.h>
#include <chrono>

namespace std {
    namespace threadimpl {
        struct thread_start_context {
            hw_spinlock _lock;
            bool done;
            bool detached;

            thread_start_context() : _lock(), done(false), detached(false) {
            }
            thread_start_context(const thread_start_context &) = delete;
            thread_start_context(thread_start_context &&) = delete;
            thread_start_context &operator =(const thread_start_context &) = delete;
            thread_start_context &operator =(thread_start_context &&) = delete;
            virtual ~thread_start_context() {
                std::lock_guard lock(_lock);
                if (!done) {
                    wild_panic("Destroying before done is UB");
                }
            }
            virtual void invoke() {
            }
            virtual bool is_done() {
                return false;
            }
            virtual void detach() {
            }
        };
        template <class Function> struct thread_start_context_impl : thread_start_context {
            Function f;
            thread_start_context_impl(Function f) : thread_start_context(), f(f) {
            }
            ~thread_start_context_impl() {
            }
            void invoke() override {
                f();

                bool detached = false;
                {
                    std::lock_guard lock(this->_lock);
                    this->done = true;
                    detached = this->detached;
                }
                if (detached) {
                    delete this;
                }
            }
            bool is_done() override {
                std::lock_guard lock(this->_lock);
                return done;
            }
            void detach() override {
                bool done = false;
                {
                    std::lock_guard lock(this->_lock);
                    done = this->done;
                    this->detached = true;
                }
                if (done) {
                    delete this;
                }
            }
        };
    }
    class thread {
    private:
        threadimpl::thread_start_context *start_context;
        uint32_t id;

        uint32_t start();
    public:
        thread() : start_context(nullptr), id(0) {
        }
        template <class Function> thread(Function && f) : thread() {
            start_context = new threadimpl::thread_start_context_impl<Function>(f);
            id = start();
        }
        thread(const thread &) = delete;
        thread(thread &&mv) : start_context(mv.start_context), id(mv.id) {
            mv.start_context = nullptr;
            mv.id = 0;
        }
        thread &operator =(const thread &) = delete;
        thread &operator =(thread &&mv) {
            if (this == &mv) {
                return *this;
            }
            if (start_context != nullptr) {
                delete start_context;
                start_context = nullptr;
            }
            start_context = mv.start_context;
            id = mv.id;
            mv.start_context = nullptr;
            mv.id = 0;
            return *this;
        }
        ~thread() {
            if (start_context != nullptr) {
                delete start_context;
                start_context = nullptr;
            }
        }

        void join();
        void detach();
    };

    namespace this_thread {

        template<class Rep, class Period>
        void sleep_for(const std::chrono::duration<Rep, Period>& sleep_duration) {
            {
                std::chrono::seconds seconds = std::chrono::duration_cast < std::chrono::seconds > (sleep_duration);
                if (seconds.count() > 120) {
                    get_scheduler()->sleep(seconds.count());
                    return;
                }
            }
            {
                std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_duration);
                if (ms.count() > 1000) {
                    get_scheduler()->millisleep(ms.count());
                    return;
                }
            }
            {
                std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(sleep_duration);
                if (us.count() > 1000) {
                    get_scheduler()->usleep(us.count());
                    return;
                }
            }
            std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds> (sleep_duration);
            get_scheduler()->nanosleep(ns.count());
        }

        inline void set_name(const std::string &name) {
            get_scheduler()->set_name(name);
        }
    }
}

#endif //JEOKERNEL_THREAD_H
