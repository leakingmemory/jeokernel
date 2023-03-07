//
// Created by sigsegv on 3/7/23.
//

#ifndef JEOKERNEL_BLOCKDEV_NODE_H
#define JEOKERNEL_BLOCKDEV_NODE_H

#include <memory>
#include <string>

class fileitem;
class blockdev_with_partitions;

class blockdev_node {
private:
    std::string name;
    std::shared_ptr<fileitem> node;
    std::shared_ptr<blockdev_with_partitions> blockdev;
public:
    blockdev_node(const std::string &name, std::shared_ptr<blockdev_with_partitions> blockdev);
    blockdev_node(const blockdev_node &) = delete;
    static void swap(blockdev_node &node1, blockdev_node &node2);
    blockdev_node(blockdev_node &&mv);
    blockdev_node &operator =(const blockdev_node &) = delete;
    blockdev_node &operator =(blockdev_node &&);
    ~blockdev_node();
    [[nodiscard]] std::string GetName() const {
        return name;
    }
    [[nodiscard]] std::shared_ptr<blockdev_with_partitions> GetBlockdev() const {
        return blockdev;
    }
};


#endif //JEOKERNEL_BLOCKDEV_NODE_H
