//
// Created by sigsegv on 9/9/22.
//

#ifndef JEOKERNEL_SYSCALLASYNCTHREAD_H
#define JEOKERNEL_SYSCALLASYNCTHREAD_H

#include <concurrency/hw_spinlock.h>
#include <concurrency/raw_semaphore.h>
#include <string>
#include <vector>
#include <functional>

namespace std {
    class thread;
}

class SyscallAsyncThread {
private:
    std::string name;
    hw_spinlock mtx;
    raw_semaphore sema;
    std::vector<std::function<void ()>> queue;
    std::thread *thr;
    bool stop;
public:
    SyscallAsyncThread(const std::string &name) : name(name), mtx(), sema(-1), queue(), thr(nullptr), stop(false) {}
    ~SyscallAsyncThread();
    SyscallAsyncThread(const SyscallAsyncThread &) = delete;
    SyscallAsyncThread(SyscallAsyncThread &&) = delete;
    SyscallAsyncThread &operator =(const SyscallAsyncThread &) = delete;
    SyscallAsyncThread &operator =(SyscallAsyncThread &&) = delete;
    void Queue(const std::function<void ()> &f);
};


#endif //JEOKERNEL_SYSCALLASYNCTHREAD_H
