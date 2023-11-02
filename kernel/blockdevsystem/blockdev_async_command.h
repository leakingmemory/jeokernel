//
// Created by sigsegv on 3/30/22.
//

#ifndef JEOKERNEL_BLOCKDEV_ASYNC_COMMAND_H
#define JEOKERNEL_BLOCKDEV_ASYNC_COMMAND_H

#include <core/blockdevsystem.h>
#include <concurrency/mul_semaphore.h>

class blockdev_async_command : public blockdev_command {
private:
    mul_semaphore semaphore;
    hw_spinlock &_lock;
    std::shared_ptr<blockdev_block> result;
public:
    blockdev_async_command(hw_spinlock &_lock, size_t blocknum, size_t blocks) : blockdev_command(blocknum, blocks), semaphore(), _lock(_lock), result() {}
    blockdev_async_command(hw_spinlock &_lock, size_t blocknum, const void *buffer, size_t blocks) : blockdev_command(blocknum, buffer, blocks), semaphore(), _lock(_lock), result() {}
    void Accept(std::shared_ptr<blockdev_block> result) override;
    std::shared_ptr<blockdev_block> Await();
};


#endif //JEOKERNEL_BLOCKDEV_ASYNC_COMMAND_H
