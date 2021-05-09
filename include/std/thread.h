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

            thread_start_context() : _lock(), done(false) {
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
        };
        template <class Function> struct thread_start_context_impl : thread_start_context {
            Function f;
            thread_start_context_impl(Function f) : thread_start_context(), f(f) {
            }
            ~thread_start_context_impl() {
            }
            void invoke() override {
                f();

                critical_section cli{};
                std::lock_guard lock(this->_lock);
                this->done = true;
            }
        };
    }
    class thread {
    private:
        threadimpl::thread_start_context *start_context;

        void start();
    public:
        thread() : start_context(nullptr) {
        }
        template <class Function> thread(Function && f) : thread() {
            start_context = new threadimpl::thread_start_context_impl<Function>(f);
            start();
        }
    };
}

#endif //JEOKERNEL_THREAD_H
