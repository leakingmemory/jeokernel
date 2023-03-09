//
// Created by sigsegv on 3/8/23.
//

#ifndef JEOKERNEL_DEVFS_IMPL_H
#define JEOKERNEL_DEVFS_IMPL_H

#include <devfs/devfs.h>
#include <concurrency/hw_spinlock.h>
#include <files/directory.h>

class devfs_node_impl : public fileitem, public devfs_node {
private:
    uint32_t mode;
public:
    devfs_node_impl(uint32_t mode);
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};

class devfs_directory_impl : public directory, public devfs_directory {
private:
    hw_spinlock mtx;
    std::vector<std::shared_ptr<directory_entry>> entries;
    uint32_t mode;
public:
    devfs_directory_impl(uint32_t mode);
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    entries_result Entries() override;
    void Add(const std::string &name, std::shared_ptr<fileitem> node) override;
    void Remove(std::shared_ptr<fileitem> node) override;
};

#endif //JEOKERNEL_DEVFS_IMPL_H
