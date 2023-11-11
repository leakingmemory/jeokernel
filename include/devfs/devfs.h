//
// Created by sigsegv on 9/21/22.
//

#ifndef JEOKERNEL_DEVFS_H
#define JEOKERNEL_DEVFS_H

#include <memory>
#include <string>
#include <files/fsresource.h>

class fileitem;

class devfs_node : public fsresource<devfs_node> {
public:
    devfs_node(fsresourcelockfactory &lockfactory) : fsresource<devfs_node>(lockfactory) {}
    virtual ~devfs_node() = default;
    devfs_node *GetResource() override;
};

class devfs_directory : public fsresource<devfs_directory> {
public:
    devfs_directory(fsresourcelockfactory &lockfactory) : fsresource<devfs_directory>(lockfactory) {}
    virtual ~devfs_directory() = default;
    devfs_directory *GetResource() override;
    virtual void Add(const std::string &name, std::shared_ptr<devfs_node> node) = 0;
    virtual void Remove(std::shared_ptr<devfs_node> node) = 0;
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
