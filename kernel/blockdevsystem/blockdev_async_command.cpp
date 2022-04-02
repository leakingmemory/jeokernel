//
// Created by sigsegv on 3/30/22.
//

#include "blockdev_async_command.h"
#include <mutex>

void blockdev_async_command::Accept(std::shared_ptr<blockdev_block> result) {
    std::lock_guard lock{_lock};
    this->result = result;
    semaphore.release();
}

std::shared_ptr<blockdev_block> blockdev_async_command::Await() {
    semaphore.acquire(120*1000);
    std::shared_ptr<blockdev_block> result{};
    {
        std::lock_guard lock{_lock};
        result = this->result;
    }
    return result;
}