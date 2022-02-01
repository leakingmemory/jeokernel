//
// Created by sigsegv on 2/1/22.
//

#ifndef JEOKERNEL_MUL_SEMAPHORE_H
#define JEOKERNEL_MUL_SEMAPHORE_H

#include <concurrency/hw_spinlock.h>
#include <vector>

class waiting_mul_semaphore;

class mul_semaphore {
private:
    hw_spinlock spinlock;
    std::vector<waiting_mul_semaphore *> waiting;
    unsigned int release_count;
public:
    mul_semaphore();
    void acquire(uint64_t timeout_millis);
    void release();
};

#endif //JEOKERNEL_MUL_SEMAPHORE_H
