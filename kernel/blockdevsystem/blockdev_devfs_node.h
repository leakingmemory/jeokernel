//
// Created by sigsegv on 3/7/23.
//

#ifndef JEOKERNEL_BLOCKDEV_DEVFS_NODE_H
#define JEOKERNEL_BLOCKDEV_DEVFS_NODE_H

#include <devfs/devfs.h>
#include <kfs/kfiles.h>

class blockdev_devfs_node : public devfs_node, public kstatable {
public:
    blockdev_devfs_node() : devfs_node(00600) {}
    void stat(struct stat64 &st) const override;
    void stat(struct statx &st) const override;
};


#endif //JEOKERNEL_BLOCKDEV_DEVFS_NODE_H
