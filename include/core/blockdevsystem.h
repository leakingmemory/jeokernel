//
// Created by sigsegv on 3/21/22.
//

#ifndef JEOKERNEL_BLOCKDEVSYSTEM_H
#define JEOKERNEL_BLOCKDEVSYSTEM_H

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <concurrency/raw_semaphore.h>
#include <concurrency/hw_spinlock.h>

class blockdev;
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

class blockdevsystem_impl;

class blockdev_with_partitions {
    friend blockdevsystem_impl;
private:
    std::shared_ptr<blockdev> bdev;
    std::vector<blockdev_with_partitions *> subs;
public:
    blockdev_with_partitions(std::shared_ptr<blockdev> bdev);
    void FindSubs(const std::string &parent_name, blockdevsystem_impl &);
    void RemoveSubs(blockdevsystem_impl &);
    [[nodiscard]] std::shared_ptr<blockdev> GetBlockdev() const {
        return bdev;
    }
};

class blockdev_interface {
private:
    hw_spinlock &_lock;
    std::vector<std::shared_ptr<blockdev_command>> queue;
    raw_semaphore semaphore;
    size_t blocksize;
    size_t numBlocks;
    bool stop;
public:
    blockdev_interface(hw_spinlock &_lock) : _lock(_lock), queue(), semaphore(-1), blocksize(0), stop(false) {}
    void SetBlocksize(std::size_t size);
    size_t GetBlocksize();
    void SetNumBlocks(std::size_t numBlocks);
    size_t GetNumBlocks();
    std::shared_ptr<blockdev_command> NextCommand();
    void Stop();
    void Submit(std::shared_ptr<blockdev_command> command);
};

enum class blockdevsystem_status {
    SUCCESS,
    IO_ERROR,
    INTEGRITY_ERROR,
    NOT_SUPPORTED_FS_FEATURE,
    INVALID_REQUEST
};

template <typename T> struct blockdevsystem_get_node_result {
    std::shared_ptr<T> node;
    blockdevsystem_status status;
};


class filesystem;
class directory;

class blockdevsystem {
public:
    blockdevsystem();
    virtual std::shared_ptr<blockdev_interface> CreateInterface() = 0;
    virtual void Add(const std::string &name, blockdev_interface *dev) = 0;
    virtual void Remove(blockdev_interface *dev) = 0;
    virtual std::vector<std::string> GetBlockdevices() = 0;
    virtual std::shared_ptr<blockdev> GetBlockdevice(const std::string &name) = 0;
    virtual std::shared_ptr<filesystem> OpenFilesystem(const std::string &provider_name, std::shared_ptr<blockdev> bdev) = 0;
    virtual std::shared_ptr<filesystem> OpenFilesystem(const std::string &provider_name) = 0;
    virtual blockdevsystem_get_node_result<directory> GetRootDirectory(std::shared_ptr<filesystem> fs) = 0;
};

void init_blockdevsystem();
blockdevsystem &get_blockdevsystem();

#endif //JEOKERNEL_BLOCKDEVSYSTEM_H
