//
// Created by sigsegv on 3/21/22.
//

#include <core/blockdevsystem.h>
#include <blockdevs/blockdev.h>
#include <vector>
#include <concurrency/hw_spinlock.h>
#include <mutex>
#include "blockdev_async_command.h"
#include <sstream>
#include <klogger.h>
#include <blockdevs/parttable_readers.h>

static parttable_readers *parttableReaders = nullptr;
static blockdevsystem *blockdevSystem = nullptr;


blockdevsystem::blockdevsystem() {}

class blockdev_container : public blockdev {
public:
    virtual ~blockdev_container() = default;
    virtual blockdev &GetBlockdev() {
        return *this;
    }
    virtual blockdev_interface *GetBlockdevInterface() const = 0;
    virtual void DetachBlockdevInterface() = 0;
    std::size_t GetBlocksize() const override = 0;
    std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const override = 0;
};

class blockdevsystem_impl;

class hw_blockdev : public blockdev_container {
private:
    hw_spinlock &_lock;
    blockdev_interface *bdev;
public:
    hw_blockdev(hw_spinlock &_lock, blockdev_interface *bdev);
    ~hw_blockdev() override;
    blockdev_interface *GetBlockdevInterface() const override;
    void DetachBlockdevInterface() override;
    virtual std::size_t GetBlocksize() const;
    virtual std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const;
};

class blockdevsystem_impl : public blockdevsystem {
private:
    hw_spinlock _lock;
    std::vector<std::shared_ptr<blockdev_container>> blockdevs;
public:
    blockdevsystem_impl();
    std::shared_ptr<blockdev_interface> CreateInterface() override;
    void Add(blockdev_interface *bdev) override;
    void Remove(blockdev_interface *bdev) override;
};

hw_blockdev::hw_blockdev(hw_spinlock &_lock, blockdev_interface *bdev) : _lock(_lock), bdev(bdev) {}

hw_blockdev::~hw_blockdev() {}

blockdev_interface *hw_blockdev::GetBlockdevInterface() const {
    return bdev;
}

void hw_blockdev::DetachBlockdevInterface() {
    bdev = nullptr;
}

std::size_t hw_blockdev::GetBlocksize() const {
    return bdev->GetBlocksize();
}

std::shared_ptr<blockdev_block> hw_blockdev::ReadBlock(size_t blocknum, size_t blocks) const {
    std::shared_ptr<blockdev_async_command> command = std::make_shared<blockdev_async_command>(_lock, blocknum, blocks);
    {
        std::lock_guard lock{_lock};
        bdev->Submit(command);
    }
    return command->Await();
}

blockdevsystem_impl::blockdevsystem_impl() : _lock(), blockdevs() {}

std::shared_ptr<blockdev_interface> blockdevsystem_impl::CreateInterface() {
    return std::make_shared<blockdev_interface>(_lock);
}

void blockdevsystem_impl::Add(blockdev_interface *bdev) {
    std::shared_ptr<hw_blockdev> hwbdev{};

    {
        std::lock_guard lock{_lock};
        hwbdev = std::make_shared<hw_blockdev>(_lock, bdev);
        blockdevs.push_back(hwbdev);
    }

    auto parttable = parttableReaders->ReadParttable(hwbdev);
    std::stringstream str{};
    str << parttable->GetTableType() << " with blocksize " << parttable->GetBlockSize()
        << " and signature " << std::hex << parttable->GetSignature() << "\n" << std::dec;
    for (auto part : parttable->GetEntries()) {
        str << " : offset " << part->GetOffset() << " size " << part->GetSize() << " type " << std::hex << part->GetType() << std::dec << "\n";
    }
    get_klogger() << str.str().c_str();
}

void blockdevsystem_impl::Remove(blockdev_interface *bdev) {
    std::lock_guard lock{_lock};
    auto iter = blockdevs.begin();
    while (iter != blockdevs.end()) {
        if (((*iter)->GetBlockdevInterface()) == bdev) {
            (*iter)->DetachBlockdevInterface();
            blockdevs.erase(iter);
            return;
        }
        ++iter;
    }
}

blockdevsystem &get_blockdevsystem() {
    if (parttableReaders == nullptr) {
        parttableReaders = new parttable_readers;
    }
    if (blockdevSystem == nullptr) {
        blockdevSystem = new blockdevsystem_impl;
    }
    return *blockdevSystem;
}