//
// Created by sigsegv on 3/21/22.
//

#ifndef JEOKERNEL_BLOCKDEVSYSTEM_H
#define JEOKERNEL_BLOCKDEVSYSTEM_H

#include <cstdint>
#include <memory>
#include <vector>
#include <concurrency/raw_semaphore.h>
#include <concurrency/hw_spinlock.h>

class blockdev_block;

class blockdev_command {
private:
    size_t blocknum;
    size_t blocks;
public:
    blockdev_command(size_t blocknum, size_t blocks) : blocknum(blocknum), blocks(blocks) {}
    size_t Blocknum() {
        return blocknum;
    }
    size_t Blocks() {
        return blocks;
    }
    virtual void Accept(std::shared_ptr<blockdev_block> result) = 0;
};

class blockdev_interface {
private:
    hw_spinlock &_lock;
    std::vector<std::shared_ptr<blockdev_command>> queue;
    raw_semaphore semaphore;
    size_t blocksize;
    bool stop;
public:
    blockdev_interface(hw_spinlock &_lock) : _lock(_lock), queue(), semaphore(-1), blocksize(0), stop(false) {}
    void SetBlocksize(std::size_t size);
    size_t GetBlocksize();
    std::shared_ptr<blockdev_command> NextCommand();
    void Stop();
    void Submit(std::shared_ptr<blockdev_command> command);
};

class blockdevsystem {
public:
    blockdevsystem();
    virtual std::shared_ptr<blockdev_interface> CreateInterface() = 0;
    virtual void Add(blockdev_interface *dev) = 0;
    virtual void Remove(blockdev_interface *dev) = 0;
};

blockdevsystem &get_blockdevsystem();

#endif //JEOKERNEL_BLOCKDEVSYSTEM_H
