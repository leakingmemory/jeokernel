//
// Created by sigsegv on 9/21/22.
//

#ifndef JEOKERNEL_DEVFS_H
#define JEOKERNEL_DEVFS_H

#include <concurrency/hw_spinlock.h>
#include <files/directory.h>

class devfs_node : public fileitem {
private:
    uint32_t mode;
public:
    devfs_node(uint32_t mode);
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};

class devfs_directory : public directory {
private:
    hw_spinlock mtx;
    std::vector<std::shared_ptr<directory_entry>> entries;
    uint32_t mode;
public:
    devfs_directory(uint32_t mode);
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    entries_result Entries() override;
    void Add(const std::string &name, std::shared_ptr<fileitem> node);
    void Remove(std::shared_ptr<fileitem> node);
};

class devfs {
private:
    std::shared_ptr<devfs_directory> root;
public:
    devfs();
    std::shared_ptr<devfs_directory> GetRoot();
};

std::shared_ptr<devfs> GetDevfs();

#endif //JEOKERNEL_DEVFS_H
