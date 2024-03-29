//
// Created by sigsegv on 3/21/22.
//

#include <core/blockdevsystem.h>
#include <blockdevs/blockdev.h>
#include <vector>
#include <concurrency/hw_spinlock.h>
#include <mutex>
#include "blockdev_async_command.h"
#include <blockdevs/offset_blockdev.h>
#include <sstream>
#include <klogger.h>
#include <blockdevs/parttable_readers.h>
#include <tuple>
#include <filesystems/filesystem.h>
#include <devfs/devfsinit.h>
#include <procfs/procfs.h>
#include <files/fsresource.h>
#include "blockdev_node.h"

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
    uintptr_t GetDevId() const override = 0;
    std::size_t GetBlocksize() const override = 0;
    std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const override = 0;
};

class blockdevsystem_impl;

class hw_blockdev : public blockdev_container {
private:
    hw_spinlock &_lock;
    blockdev_interface *bdev;
    uintptr_t sys_dev_id;
public:
    hw_blockdev(hw_spinlock &_lock, blockdev_interface *bdev, uintptr_t sys_dev_id);
    ~hw_blockdev() override;
    blockdev_interface *GetBlockdevInterface() const override;
    void DetachBlockdevInterface() override;
    uintptr_t GetDevId() const override;
    std::size_t GetBlocksize() const override;
    std::size_t GetNumBlocks() const override;
    std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const override;
    size_t WriteBlock(const void *data, size_t blocknum, size_t blocks) const override;
};

class blockdevsystem_impl;

class blockdevsystem_impl : public blockdevsystem {
    friend blockdev_with_partitions;
private:
    hw_spinlock _lock;
    std::vector<std::shared_ptr<blockdev_container>> blockdevs;
    std::vector<blockdev_node> availBlockdevs;
    uintptr_t sysDevIds;
public:
    blockdevsystem_impl();
    std::shared_ptr<blockdev_interface> CreateInterface() override;
    void Add(const std::string &name, blockdev_interface *bdev) override;
    void Remove(blockdev_interface *bdev) override;
private:
    void RemoveSub(blockdev_with_partitions *subbdev);
public:
    std::vector<std::string> GetBlockdevices() override;
    std::shared_ptr<blockdev> GetBlockdevice(const std::string &name) override;
    std::shared_ptr<filesystem> OpenFilesystem(const std::string &provider_name, std::shared_ptr<blockdev> bdev) override;
    std::shared_ptr<filesystem> OpenFilesystem(const std::string &provider_name) override;
};

blockdev_with_partitions::blockdev_with_partitions(std::shared_ptr<blockdev> bdev)
    : bdev(bdev), subs() {
}

void blockdev_with_partitions::FindSubs(const std::string &parent_name, blockdevsystem_impl &system) {
    auto parttable = parttableReaders->ReadParttable(bdev);
    if (!parttable) {
        return;
    }
    std::stringstream str{};
    str << parttable->GetTableType() << " with blocksize " << parttable->GetBlockSize()
        << " and signature " << std::hex << parttable->GetSignature() << "\n" << std::dec;
    int index = 0;
    for (auto part : parttable->GetEntries()) {
        auto dev_id = (bdev->GetDevId() << 12) + index;
        std::shared_ptr<blockdev> partdev = std::make_shared<offset_blockdev>(bdev, part->GetOffset(), part->GetSize(), dev_id);
        std::shared_ptr<blockdev_with_partitions> partwp = std::make_shared<blockdev_with_partitions>(partdev);
        std::stringstream name{};
        name << parent_name << "p" << ++index;
        str << name.str() << ": offset " << part->GetOffset() << " size " << part->GetSize() << " type " << std::hex << part->GetType() << std::dec << "\n";
        {
            std::lock_guard lock{system._lock};
            subs.push_back(&(*partwp));
            system.availBlockdevs.emplace_back(name.str(), partwp);
        }
    }
    get_klogger() << str.str().c_str();
}

void blockdev_with_partitions::RemoveSubs(blockdevsystem_impl &system) {
    std::vector<blockdev_with_partitions *> subs{};
    {
        std::lock_guard lock{system._lock};
        for (auto *sub: this->subs) {
            subs.push_back(sub);
        }
        this->subs.clear();
    }
    for (auto *sub : subs) {
        system.RemoveSub(sub);
    }
}

hw_blockdev::hw_blockdev(hw_spinlock &_lock, blockdev_interface *bdev, uintptr_t sys_dev_id) : _lock(_lock), bdev(bdev), sys_dev_id(sys_dev_id) {}

hw_blockdev::~hw_blockdev() {}

blockdev_interface *hw_blockdev::GetBlockdevInterface() const {
    return bdev;
}

void hw_blockdev::DetachBlockdevInterface() {
    bdev = nullptr;
}

uintptr_t hw_blockdev::GetDevId() const {
    return sys_dev_id;
}

std::size_t hw_blockdev::GetBlocksize() const {
    return bdev->GetBlocksize();
}

std::size_t hw_blockdev::GetNumBlocks() const {
    return bdev->GetNumBlocks();
}

std::shared_ptr<blockdev_block> hw_blockdev::ReadBlock(size_t blocknum, size_t blocks) const {
    std::shared_ptr<blockdev_async_command> command = std::make_shared<blockdev_async_command>(_lock, blocknum, blocks);
    {
        std::lock_guard lock{_lock};
        bdev->Submit(command);
    }
    return command->Await();
}

size_t hw_blockdev::WriteBlock(const void *data, size_t blocknum, size_t blocks) const {
    std::shared_ptr<blockdev_async_command> command = std::make_shared<blockdev_async_command>(_lock, blocknum, data, blocks);
    {
        std::lock_guard lock{_lock};
        bdev->Submit(command);
    }
    auto result = command->Await();
    return result->Size() / GetBlocksize();
}

blockdevsystem_impl::blockdevsystem_impl() : _lock(), blockdevs(), sysDevIds(0) {}

std::shared_ptr<blockdev_interface> blockdevsystem_impl::CreateInterface() {
    return std::make_shared<blockdev_interface>(_lock);
}

void blockdevsystem_impl::Add(const std::string &name, blockdev_interface *bdev) {
    std::shared_ptr<blockdev_with_partitions> partitions{};
    {
        std::lock_guard lock{_lock};
        std::shared_ptr<hw_blockdev> hwbdev = std::make_shared<hw_blockdev>(_lock, bdev, ++sysDevIds);
        blockdevs.push_back(hwbdev);
        partitions = std::make_shared<blockdev_with_partitions>(hwbdev);
        availBlockdevs.emplace_back(name, partitions);
    }
    partitions->FindSubs(name, *this);
}

void blockdevsystem_impl::Remove(blockdev_interface *bdev) {
    std::shared_ptr<blockdev_with_partitions> sub{};
    {
        blockdev *bdevptr = nullptr;
        std::lock_guard lock{_lock};
        {
            auto iter = blockdevs.begin();
            while (iter != blockdevs.end()) {
                if (((*iter)->GetBlockdevInterface()) == bdev) {
                    (*iter)->DetachBlockdevInterface();
                    bdevptr = &(**iter);
                    blockdevs.erase(iter);
                    break;
                }
                ++iter;
            }
        }
        auto iter = availBlockdevs.begin();
        while (iter != availBlockdevs.end()) {
            blockdev_with_partitions *ptr = &(*((*iter).GetBlockdev()));
            if (&(*(ptr->bdev)) == bdevptr) {
                break;
            }
            ++iter;
        }
        if (iter == availBlockdevs.end()) {
            return;
        }
        sub = (*iter).GetBlockdev();
        availBlockdevs.erase(iter);
    }
    sub->RemoveSubs(*this);
}

void blockdevsystem_impl::RemoveSub(blockdev_with_partitions *subbdev) {
    std::shared_ptr<blockdev_with_partitions> sub{};
    {
        std::lock_guard lock{_lock};
        auto iter = availBlockdevs.begin();
        while (iter != availBlockdevs.end()) {
            blockdev_with_partitions *ptr = &(*((*iter).GetBlockdev()));
            if (ptr == subbdev) {
                break;
            }
            ++iter;
        }
        if (iter == availBlockdevs.end()) {
            return;
        }
        sub = (*iter).GetBlockdev();
        availBlockdevs.erase(iter);
    }
    sub->RemoveSubs(*this);
}

std::vector<std::string> blockdevsystem_impl::GetBlockdevices() {
    std::vector<std::string> names{};
    std::lock_guard lock{_lock};
    for (const auto &t : availBlockdevs) {
        names.push_back(t.GetName());
    }
    return names;
}

std::shared_ptr<blockdev> blockdevsystem_impl::GetBlockdevice(const std::string &name) {
    std::lock_guard lock{_lock};
    for (auto &t : availBlockdevs) {
        if (t.GetName() == name) {
            return t.GetBlockdev()->bdev;
        }
    }
    return {};
}

class blockdevsystem_fsresourcelock : public fsresourcelock {
private:
    hw_spinlock spinlock{};
public:
    void lock() override {
        spinlock.lock();
    }
    void unlock() override {
        spinlock.unlock();
    }
};

class blockdevsystem_fsresourcelockfactory : public fsresourcelockfactory {
public:
    std::shared_ptr<fsresourcelock> Create() override;
};

std::shared_ptr<fsresourcelock> blockdevsystem_fsresourcelockfactory::Create() {
    return std::make_shared<blockdevsystem_fsresourcelock>();
}

std::shared_ptr<filesystem>
blockdevsystem_impl::OpenFilesystem(const std::string &provider_name, std::shared_ptr<blockdev> bdev) {
    return open_filesystem(std::make_shared<blockdevsystem_fsresourcelockfactory>(), provider_name, bdev);
}

std::shared_ptr<filesystem> blockdevsystem_impl::OpenFilesystem(const std::string &provider_name) {
    return open_filesystem(provider_name);
}

void init_blockdevsystem() {
    parttableReaders = new parttable_readers;
    blockdevSystem = new blockdevsystem_impl;

    init_filesystem_providers();
    register_filesystem_providers();
    InstallDevfs();
    InstallProcfs();
}

blockdevsystem &get_blockdevsystem() {
    return *blockdevSystem;
}