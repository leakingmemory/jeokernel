//
// Created by sigsegv on 4/10/22.
//

#ifndef JEOKERNEL_CALLBACKLATCH_H
#define JEOKERNEL_CALLBACKLATCH_H

#include <concurrency/mul_semaphore.h>
#include <mutex>

class CallbackLatch {
private:
    mul_semaphore sema;
    std::mutex mtx;
    bool success;
public:
    CallbackLatch();
    void open();
    bool wait(uint64_t millis);
};


#endif //JEOKERNEL_CALLBACKLATCH_H
