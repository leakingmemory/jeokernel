//
// Created by sigsegv on 3/7/23.
//

#include "blockdev_node.h"
#include <core/blockdev_devfs_node.h>
#include <core/blockdevsystem.h>
#include <devfs/devfs.h>


blockdev_node::blockdev_node(const std::string &name, std::shared_ptr<blockdev_with_partitions> blockdev) :
        name(name), node(), blockdev(blockdev) {
    node = blockdev_devfs_node::Create(blockdev->GetBlockdev())->AsDevfsNode();
    GetDevfs()->GetRoot()->Add(name, node);
}

void blockdev_node::swap(blockdev_node &node1, blockdev_node &node2) {
    std::string name{node1.name};
    auto node = node1.node;
    auto blockdev = node1.blockdev;
    node1.name = node2.name;
    node1.node = node2.node;
    node1.blockdev = node2.blockdev;
    node2.name = name;
    node2.node = node;
    node2.blockdev = blockdev;
}

blockdev_node::blockdev_node(blockdev_node &&mv) : name(mv.name), node(mv.node), blockdev(mv.blockdev) {
    mv.node = {};
}

blockdev_node &blockdev_node::operator=(blockdev_node &&mv) {
    blockdev_node sw{std::move(mv)};
    swap(*this, sw);
    return *this;
}

blockdev_node::~blockdev_node() {
    if (node) {
        GetDevfs()->GetRoot()->Remove(node);
    }
}