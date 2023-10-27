//
// Created by sigsegv on 2/9/22.
//

#ifndef FSBITS_BLOCKDEV_H
#define FSBITS_BLOCKDEV_H

#include <memory>
#include <blockdevs/blockdev_block.h>

class blockdev {
public:
    virtual ~blockdev() { }
    virtual uintptr_t GetDevId() const = 0;
    virtual std::size_t GetBlocksize() const = 0;
    virtual std::size_t GetNumBlocks() const = 0;
    virtual std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const = 0;
    virtual size_t WriteBlock(const void *data, size_t blocknum, size_t blocks) const = 0;
};

#endif //FSBITS_BLOCKDEV_H
