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

#define RING_SIZE 1024

class framebuffer_kcons_with_worker_thread : public KLogger {
private:
    framebuffer_kcons_cmd *command_ring[RING_SIZE];
    unsigned int command_ring_extract;
    unsigned int command_ring_insert;
    std::shared_ptr<framebuffer_kconsole> targetObject;
    hw_spinlock spinlock;
    raw_semaphore semaphore;
    bool terminate;
    std::thread worker;
public:
    explicit framebuffer_kcons_with_worker_thread(std::shared_ptr<framebuffer_kconsole> targetObject);
    ~framebuffer_kcons_with_worker_thread() override;
    void WorkerThread();

    bool InsertCommand(framebuffer_kcons_cmd *cmd);

    void print_at(uint8_t col, uint8_t row, const char *str) override;
    framebuffer_kcons_with_worker_thread & operator << (const char *str) override;
};


#endif //JEOKERNEL_FRAMEBUFFER_KCONS_WITH_WORKER_THREAD_H
