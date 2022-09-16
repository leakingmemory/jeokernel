//
// Created by sigsegv on 2/12/22.
//

#include <blockdevs/offset_blockdev.h>

uintptr_t offset_blockdev::GetDevId() const {
    return sys_dev_id;
}

std::size_t offset_blockdev::GetBlocksize() const {
    return upstream->GetBlocksize();
}

std::size_t offset_blockdev::GetNumBlocks() const {
    return size;
}

std::shared_ptr<blockdev_block> offset_blockdev::ReadBlock(size_t blocknum, size_t blocks) const {
    if (blocknum >= size) {
        return {};
    }
    if ((blocknum + blocks) >= size) {
        blocks = size - blocknum;
    }
    return upstream->ReadBlock(blocknum + offset, blocks);
}
