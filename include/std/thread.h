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
                critical_section cli{};
                std::lock_guard lock(_lock);
                if (!done) {
                    wild_panic("Destroying before done is UB");
                }
            }
            virtual void invoke() {
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
                    critical_section cli{};
                    std::lock_guard lock(this->_lock);
                    this->done = true;
                    detached = this->detached;
                }
                if (detached) {
                    delete this;
                }
            }
            void detach() override {
                bool done = false;
                {
                    critical_section cli{};
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
        ~thread() {
            if (start_context != nullptr) {
                delete start_context;
                start_context = nullptr;
            }
        }

        void join();
        void detach();
    };
}

#endif //JEOKERNEL_THREAD_H
