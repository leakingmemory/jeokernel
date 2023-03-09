//
// Created by sigsegv on 9/21/22.
//

#ifndef JEOKERNEL_DEVFS_H
#define JEOKERNEL_DEVFS_H

#include <memory>
#include <string>

class fileitem;

class devfs_node {
public:
    virtual ~devfs_node() = default;
};

class devfs_directory {
public:
    virtual ~devfs_directory() = default;
    virtual void Add(const std::string &name, std::shared_ptr<fileitem> node) = 0;
    virtual void Remove(std::shared_ptr<fileitem> node) = 0;
};

class devfs_directory_impl;

class devfs {
private:
    std::shared_ptr<devfs_directory> root;
public:
    devfs();
    std::shared_ptr<devfs_directory> GetRoot();
};

std::shared_ptr<devfs> GetDevfs();

#endif //JEOKERNEL_DEVFS_H
