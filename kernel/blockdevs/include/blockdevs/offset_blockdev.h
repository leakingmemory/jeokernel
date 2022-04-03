//
// Created by sigsegv on 2/12/22.
//

#ifndef FSBITS_OFFSET_BLOCKDEV_H
#define FSBITS_OFFSET_BLOCKDEV_H

#include <blockdevs/blockdev.h>

class offset_blockdev : public blockdev {
private:
    std::shared_ptr<blockdev> upstream;
    std::size_t offset;
    std::size_t size;
public:
    offset_blockdev(std::shared_ptr<blockdev> upstream, std::size_t offset, std::size_t size) : upstream(upstream), offset(offset), size(size) {
    }
    std::size_t GetBlocksize() const override;
    std::size_t GetNumBlocks() const override;
    std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const override;
};

#endif //FSBITS_OFFSET_BLOCKDEV_H
