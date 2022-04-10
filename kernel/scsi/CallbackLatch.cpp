//
// Created by sigsegv on 4/10/22.
//

#include "CallbackLatch.h"

CallbackLatch::CallbackLatch() : sema(), mtx(), success(false) {}

void CallbackLatch::open() {
    {
        std::lock_guard lock{mtx};
        success = true;
    }
    sema.release();
}

bool CallbackLatch::wait(uint64_t millis) {
    sema.acquire(millis);
    bool success;
    {
        std::lock_guard lock{mtx};
        success = this->success;
    }
    return success;
}