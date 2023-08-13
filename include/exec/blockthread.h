//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_BLOCKTHREAD_H
#define JEOKERNEL_BLOCKTHREAD_H

#include <vector>
#include <thread>
#include <functional>
#include <concurrency/hw_spinlock.h>
#include <concurrency/raw_semaphore.h>

class blockthread {
private:
    hw_spinlock spinlock{};
    bool stop{false};
    std::vector<std::function<void ()>> queue{};
    raw_semaphore sema{-1};
    std::thread thr;
public:
    blockthread();
    ~blockthread();
    blockthread(const blockthread &) = delete;
    blockthread(blockthread &&) = delete;
    blockthread &operator =(const blockthread &) = delete;
    blockthread &operator =(blockthread &&) = delete;
    void Queue(uint32_t task_id, const std::function<void ()> &);
private:
    void Stop();
};

#endif //JEOKERNEL_BLOCKTHREAD_H
