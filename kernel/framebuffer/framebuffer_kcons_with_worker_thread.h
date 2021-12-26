//
// Created by sigsegv on 12/26/21.
//

#ifndef JEOKERNEL_FRAMEBUFFER_KCONS_WITH_WORKER_THREAD_H
#define JEOKERNEL_FRAMEBUFFER_KCONS_WITH_WORKER_THREAD_H

#include <klogger.h>
#include <memory>
#include <vector>
#include <thread>
#include <concurrency/raw_semaphore.h>
#include "framebuffer_kconsole.h"

class framebuffer_kcons_cmd;

class framebuffer_kcons_with_worker_thread : public KLogger {
private:
    std::vector<std::shared_ptr<framebuffer_kcons_cmd>> command;
    std::shared_ptr<framebuffer_kconsole> targetObject;
    hw_spinlock spinlock;
    raw_semaphore semaphore;
    bool terminate;
    std::thread worker;
public:
    explicit framebuffer_kcons_with_worker_thread(std::shared_ptr<framebuffer_kconsole> targetObject);
    ~framebuffer_kcons_with_worker_thread() override;
    void WorkerThread();

    void print_at(uint8_t col, uint8_t row, const char *str) override;
    framebuffer_kcons_with_worker_thread & operator << (const char *str) override;
};


#endif //JEOKERNEL_FRAMEBUFFER_KCONS_WITH_WORKER_THREAD_H
