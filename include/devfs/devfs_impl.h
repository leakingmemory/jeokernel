//
// Created by sigsegv on 3/8/23.
//

#ifndef JEOKERNEL_DEVFS_IMPL_H
#define JEOKERNEL_DEVFS_IMPL_H

#include <devfs/devfs.h>
#include <concurrency/hw_spinlock.h>
#include <files/directory.h>
#include <files/fsreferrer.h>

class devfs_fsresourcelock : public fsresourcelock {
private:
    hw_spinlock mtx{};
public:
    void lock() override;
    void unlock() override;
};

class devfs_fsresourcelockfactory : public fsresourcelockfactory {
public:
    std::shared_ptr<fsresourcelock> Create() override;
};

class devfs_node_impl : public fileitem, public devfs_node {
private:
    uint32_t mode;
protected:
    devfs_node_impl(fsresourcelockfactory &lockfactory, uint32_t mode);
public:
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};

class fsresourcelockfactory;

class devfs_directory_impl : public directory, public devfs_directory, public fsreferrer {
private:
    hw_spinlock mtx;
    std::weak_ptr<devfs_directory_impl> selfRef{};
    std::vector<std::shared_ptr<directory_entry>> entries;
    uint32_t mode;
private:
    devfs_directory_impl(fsresourcelockfactory &lockfactory, uint32_t mode);
public:
    static std::shared_ptr<devfs_directory_impl> Create(uint32_t mode);
    std::string GetReferrerIdentifier() override;
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    entries_result Entries() override;
    void Add(const std::string &name, std::shared_ptr<devfs_node> node) override;
    void Remove(std::shared_ptr<devfs_node> node) override;
};

#endif //JEOKERNEL_DEVFS_IMPL_H
