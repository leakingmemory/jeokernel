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
    uintptr_t sys_dev_id;
public:
    offset_blockdev(std::shared_ptr<blockdev> upstream, std::size_t offset, std::size_t size, uintptr_t sys_dev_id) : upstream(upstream), offset(offset), size(size), sys_dev_id(sys_dev_id) {
    }
    uintptr_t GetDevId() const override;
    std::size_t GetBlocksize() const override;
    std::size_t GetNumBlocks() const override;
    std::shared_ptr<blockdev_block> ReadBlock(size_t blocknum, size_t blocks) const override;
    size_t WriteBlock(const void *data, size_t blocknum, size_t blocks) const override;
};

#endif //FSBITS_OFFSET_BLOCKDEV_H
