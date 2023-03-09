//
// Created by sigsegv on 3/7/23.
//

#ifndef JEOKERNEL_BLOCKDEV_DEVFS_NODE_H
#define JEOKERNEL_BLOCKDEV_DEVFS_NODE_H

#include <devfs/devfs.h>
#include <kfs/kfiles.h>

class blockdev;

class blockdev_devfs_node_impl;

class blockdev_devfs_node {
private:
    std::weak_ptr<blockdev_devfs_node_impl> impl;
    std::weak_ptr<blockdev> bdev;
public:
    static std::shared_ptr<blockdev_devfs_node> Create(const std::shared_ptr<blockdev> &bdev);

    blockdev_devfs_node(const std::shared_ptr<blockdev> bdev) : bdev(bdev) {}
    virtual ~blockdev_devfs_node() = default;
    std::shared_ptr<blockdev> GetBlockdev() {
        return bdev.lock();
    }
    static std::shared_ptr<blockdev_devfs_node> FromFileItem(const std::shared_ptr<fileitem> &);
    std::shared_ptr<fileitem> AsFileitem();
};

#endif //JEOKERNEL_BLOCKDEV_DEVFS_NODE_H
