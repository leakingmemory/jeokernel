//
// Created by sigsegv on 10/5/23.
//

#include "ext2fs_inode.h"
#include "ext2fs.h"
#include "ext2fs/ext2struct.h"
#include <strings.h>
#include <cstring>
#include <iostream>
#include <blockdevs/blockdev.h>

class ext2file_block : public blockdev_block {
    friend ext2fs_inode;
private:
    void *ptr;
    std::size_t size;
public:
    ext2file_block(std::size_t size) : ptr(nullptr), size(size) {
        ptr = malloc(size);
    }
    ext2file_block(const ext2file_block &) = delete;
    ext2file_block(ext2file_block &&) = delete;
    ext2file_block &operator =(const ext2file_block &) = delete;
    ext2file_block &operator =(ext2file_block &&) = delete;
    virtual ~ext2file_block();
    void *Pointer() const override;
    std::size_t Size() const override;
};

ext2file_block::~ext2file_block() {
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
}

void *ext2file_block::Pointer() const {
    return ptr;
}

std::size_t ext2file_block::Size() const {
    return size;
}

bool ext2fs_inode::Init() {
    ext2inode inode{};
    if (!Read(inode)) {
        return false;
    }
    bzero(&inode, sizeof(inode));
    inode.mode = 644;
    if (!Write(inode)) {
        return false;
    }
    return true;
}

bool ext2fs_inode::Read(ext2inode &inode) {
    constexpr auto blocksize = FILEPAGE_PAGE_SIZE;
    auto sz1 = sizeof(inode);
    if ((sz1 + offset) > blocksize) {
        sz1 = blocksize - offset;
        auto sz2 = sizeof(inode) - sz1;
        if (!blocks[1]) {
            return false;
        }
        memcpy(((uint8_t *) (void *) &inode) + sz1, blocks[1]->Pointer()->Pointer(), sz2);
    }
    if (!blocks[0]) {
        return false;
    }
    memcpy(&inode, ((uint8_t *) blocks[0]->Pointer()->Pointer()) + offset, sz1);
    return true;
}

bool ext2fs_inode::Write(ext2inode &inode) {
    constexpr auto blocksize = FILEPAGE_PAGE_SIZE;
    auto sz1 = sizeof(inode);
    auto fullInodeSize = inodesize > sz1 ? inodesize : sz1;
    auto extendedSize = fullInodeSize - sz1;
    if ((fullInodeSize + offset) > blocksize) {
        if ((sz1 + offset) > blocksize) {
            sz1 = blocksize - offset;
            auto sz2 = sizeof(inode) - sz1;
            if (!blocks[1]) {
                return false;
            }
            memcpy(blocks[1]->Pointer()->Pointer(), ((uint8_t *) (void *) &inode) + sz1, sz2);
            if (extendedSize > 0) {
                bzero(((uint8_t *) blocks[1]->Pointer()->Pointer()) + sz2, extendedSize);
                extendedSize = 0;
            }
        } else {
            auto clear0 = blocksize - offset - sz1;
            auto clear1 = extendedSize - clear0;
            extendedSize = clear0;
            bzero(blocks[1]->Pointer()->Pointer(), clear1);
        }
    }
    if (!blocks[0]) {
        return false;
    }
    memcpy(((uint8_t *) blocks[0]->Pointer()->Pointer()) + offset, &inode, sz1);
    if (extendedSize > 0) {
        bzero(((uint8_t *) blocks[0]->Pointer()->Pointer()) + offset + sz1, extendedSize);
    }
    return true;
}

size_t ext2fs_inode::WriteSize() const {
    return sizeof(struct ext2inode);
}

inode_read_blocks_result ext2fs_inode::ReadBlocks(uint32_t startingBlock, uint32_t startingOffset, uint32_t length) {
    std::vector<std::shared_ptr<blockdev_block>> readBlocks{};
    while (startingOffset >= blocksize) {
        ++startingBlock;
        startingOffset -= blocksize;
    }
    uint32_t numBlocks = startingOffset + length;
    uint32_t lastBlockLength = numBlocks % blocksize;
    if (lastBlockLength == 0) {
        lastBlockLength = blocksize;
    }
    numBlocks += blocksize - lastBlockLength;
    numBlocks /= blocksize;
    uint32_t firstReadLength = 0;
    for (auto blockNo = startingBlock; blockNo < (startingBlock + numBlocks); ++blockNo) {
        if (blockNo < blockRefs.size()) {
            auto fsBlock = blockRefs[blockNo];
            if (fsBlock != 0) {
                uint64_t startingAddr = fsBlock;
                startingAddr *= blocksize;
                if (blockNo == startingBlock) {
                    startingAddr += startingOffset;
                }
                uint64_t physBlock = startingAddr / bdev->GetBlocksize();
                uint64_t endingOffset = fsBlock + 1;
                if ((blockNo + 1) == (startingBlock + numBlocks)) {
                    endingOffset = startingAddr + lastBlockLength;
                } else {
                    endingOffset *= blocksize;
                }
                uint32_t offset = startingAddr % bdev->GetBlocksize();
                startingAddr = physBlock * bdev->GetBlocksize();
                uint32_t physBlocks = (endingOffset - startingAddr) / bdev->GetBlocksize();
                if (blockNo == startingBlock) {
                    firstReadLength = endingOffset - startingAddr - offset;
                }
                if (((endingOffset - startingAddr) % bdev->GetBlocksize()) != 0) {
                    ++physBlocks;
                }
#ifdef DEBUG_INODE
                std::cout << "Read file block " << blockNo << ": phys " << physBlock << " num " << physBlocks << "\n";
#endif
                auto rdBlock = bdev->ReadBlock(physBlock, physBlocks);
                if (rdBlock && offset != 0) {
                    memcpy(rdBlock->Pointer(), ((uint8_t *) rdBlock->Pointer()) + offset, blocksize);
                }
                if (!rdBlock) {
                    std::cerr << "Read error file block " << blockNo << "\n";
                    return {.block = {}, .status = filesystem_status::IO_ERROR};
                }
                readBlocks.push_back(rdBlock);
            } else {
                return {.block = {}, .status = filesystem_status::NOT_SUPPORTED_FS_FEATURE};
            }
        }
    }
    std::shared_ptr<blockdev_block> finalBlock{new ext2file_block(length)};
    bzero(finalBlock->Pointer(), length);
    int i = 0;
    uint32_t offset = 0;
    if (i < readBlocks.size()) {
        auto cp = firstReadLength;
        if (cp > length) {
            cp = length;
        }
        if (readBlocks[i]) {
#ifdef DEBUG_INODE
            std::cout << "Assemble blocks 0 - " << cp << "\n";
#endif
            memcpy(finalBlock->Pointer(), readBlocks[i]->Pointer(), cp);
        }
        offset = firstReadLength;
        ++i;
    }
    if (readBlocks.size() > 1) {
        while (i < (readBlocks.size() - 1)) {
            if (readBlocks[i]) {
#ifdef DEBUG_INODE
                std::cout << "Assemble blocks " << offset << " - " << (offset + blocksize) << "\n";
#endif
                memcpy(((uint8_t *) finalBlock->Pointer()) + offset, readBlocks[i]->Pointer(), blocksize);
            }
            offset += blocksize;
            ++i;
        }
        if (i < readBlocks.size()) {
            if (readBlocks[i]) {
#ifdef DEBUG_INODE
                std::cout << "Assemble blocks " << offset << " - " << (offset + lastBlockLength) << "\n";
#endif
                memcpy(((uint8_t *) finalBlock->Pointer()) + offset, readBlocks[i]->Pointer(), lastBlockLength);
            }
        }
    }
    return {.block = finalBlock, .status = filesystem_status::SUCCESS};
}

filesystem_status ext2fs_inode::AllocToExtend(std::size_t blki, uint32_t sizeAlloc) {
    std::shared_ptr<ext2fs> fs = this->fs.lock();
    if (!fs) {
        return filesystem_status::IO_ERROR;
    }
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    auto neededBlocks = blockRefs.size();
    lock = {};

    uint64_t startingAddr = blki;
    startingAddr *= FILEPAGE_PAGE_SIZE;
    uint64_t endingAddr = startingAddr + (sizeAlloc == 0 ? FILEPAGE_PAGE_SIZE : sizeAlloc);
    uint64_t startingBlock = startingAddr / blocksize;
    uint64_t endingBlock = endingAddr / blocksize;
    {
        uint64_t endingBlockAddr = endingBlock * blocksize;
        if (endingBlockAddr < endingAddr) {
            ++endingBlock;
        }
    }
    if (endingBlock > neededBlocks) {
        neededBlocks = endingBlock - neededBlocks;
    } else {
        neededBlocks = 0;
    }

    std::vector<uint32_t> blocks{};

    for (typeof(neededBlocks) i = 0; i < neededBlocks; i++) {
        auto allocBlockResult = fs->AllocateBlocks(1);
        if (allocBlockResult.status != filesystem_status::SUCCESS || allocBlockResult.count != 1) {
            for (auto blk : blocks) {
                fs->ReleaseBlock(blk);
            }
            return allocBlockResult.count == 1 ? allocBlockResult.status : filesystem_status::NO_AVAIL_BLOCKS;
        }
        blocks.emplace_back(allocBlockResult.block);
    }

    lock = std::make_unique<std::lock_guard<std::mutex>>(mtx);
    for (auto blk : blocks) {
        blockRefs.push_back(blk);
    }
    while (blki > blockCache.size()) {
        auto &ref = blockCache.emplace_back(new filepage());
        ref->Zero();
        ref->SetDirty(FILEPAGE_PAGE_SIZE);
    }
    if (blki >= blockCache.size()) {
        auto &ref = blockCache.emplace_back(new filepage());
        ref->Zero();
        ref->SetDirty(sizeAlloc);
    }
    return filesystem_status::SUCCESS;
}

inode_read_blocks_raw_result ext2fs_inode::ReadBlockRaw(std::size_t blki, bool alloc, uint32_t sizeAlloc) {
    if (alloc) {
        AllocToExtend(blki, sizeAlloc);
    }
    std::shared_ptr<ext2fs> fs = this->fs.lock();
    if (!fs) {
        return {.page = {}, .status = filesystem_status::IO_ERROR};
    }
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    if (blki < blockCache.size()) {
        if (!blockCache[blki]) {
            uint64_t startingAddr = blki;
            startingAddr *= FILEPAGE_PAGE_SIZE;
            uint64_t endingAddr = startingAddr + FILEPAGE_PAGE_SIZE;
            uint64_t startingBlock = startingAddr / blocksize;
            uint64_t endingBlock = endingAddr / blocksize;
            {
                uint64_t endingBlockAddr = endingBlock * blocksize;
                if (endingBlockAddr < endingAddr) {
                    ++endingBlock;
                }
            }
            auto readingOffset = startingAddr % blocksize;
            lock = {};
            auto readBlocks = ReadBlocks(startingBlock, readingOffset, FILEPAGE_PAGE_SIZE);
            if (readBlocks.block) {
                std::shared_ptr<filepage> page{new filepage()};
                memcpy(page->Pointer()->Pointer(), readBlocks.block->Pointer(), FILEPAGE_PAGE_SIZE);
                std::lock_guard lock{mtx};
                if (!blockCache[blki]) {
                    blockCache[blki] = page;
                }
                return {.page = blockCache[blki], .status = filesystem_status::SUCCESS};
            } else {
                std::cerr << "Read error for inode block " << startingBlock << "\n";
                return {.page = {}, .status = filesystem_status::IO_ERROR};
            }
        }
        return {.page = blockCache[blki], .status = filesystem_status::SUCCESS};
    }
    return {.page = {}, .status = filesystem_status::INVALID_REQUEST};
}

inode_read_block_result ext2fs_inode::ReadBlock(std::size_t blki) {
    auto block = ReadBlockRaw(blki);
    if (block.page) {
        return {.page = block.page->Pointer(), .status = block.status};
    }
    return {.page = {}, .status = block.status};
}

inode_read_block_result ext2fs_inode::ReadOrAllocBlock(std::size_t blki, std::size_t sizeAlloc) {
    auto block = ReadBlockRaw(blki, true, sizeAlloc);
    if (block.page) {
        return {.page = block.page->Pointer(), .status = block.status};
    }
    return {.page = {}, .status = block.status};
}

inode_read_bytes_result ext2fs_inode::ReadBytes(uint64_t offset, void *ptr, std::size_t length) {
    uint8_t *p = (uint8_t *) ptr;
    if ((offset + ((uint64_t) length)) > filesize) {
        if (offset >= filesize) {
            return {.size = 0, .status = filesystem_status::SUCCESS};
        }
        length = (std::size_t) (filesize - offset);
    }
    std::size_t vec = 0;
    while (vec < length) {
        uint64_t cur = offset + vec;
        uint64_t blk = cur / FILEPAGE_PAGE_SIZE;
        uint32_t blkoff = (uint32_t) (cur % FILEPAGE_PAGE_SIZE);
        uint32_t blklen = FILEPAGE_PAGE_SIZE;
        if (blklen > (length - vec)) {
            blklen = length - vec;
        }
        if (blklen > (FILEPAGE_PAGE_SIZE - blkoff)) {
            blklen = FILEPAGE_PAGE_SIZE - blkoff;
        }

        auto blkp = ReadBlock(blk);
        if (blkp.page) {
            memcpy(p + vec, ((uint8_t *) blkp.page->Pointer()) + blkoff, blklen);
        } else {
            return {.size = vec, .status = blkp.status};
        }

        vec += blklen;
    }
    return {.size = length, .status = filesystem_status::SUCCESS};
}

inode_read_bytes_result ext2fs_inode::WriteBytes(uint64_t offset, void *ptr, std::size_t length) {
    uint8_t *p = (uint8_t *) ptr;
    std::size_t vec = 0;
    while (vec < length) {
        uint64_t cur = offset + vec;
        uint64_t blk = cur / FILEPAGE_PAGE_SIZE;
        uint32_t blkoff = (uint32_t) (cur % FILEPAGE_PAGE_SIZE);
        uint32_t blklen = FILEPAGE_PAGE_SIZE;
        if (blklen > (length - vec)) {
            blklen = length - vec;
        }
        if (blklen > (FILEPAGE_PAGE_SIZE - blkoff)) {
            blklen = FILEPAGE_PAGE_SIZE - blkoff;
        }

        auto requestedLength = length - vec;

        auto blkp = ReadOrAllocBlock(blk, blkoff + blklen);
        if (blkp.page) {
            memcpy(((uint8_t *) blkp.page->Pointer()) + blkoff, p + vec, blklen);
            blkp.page->SetDirty(blkoff + blklen);
        } else {
            if ((offset + ((uint64_t) vec)) > filesize) {
                filesize = offset + ((uint64_t) vec);
                dirty = true;
            }
            return {.size = vec, .status = blkp.status};
        }

        vec += blklen;
    }
    return {.size = length, .status = filesystem_status::SUCCESS};
}

void ext2fs_inode::SetFileSize(uint64_t filesize) {
    this->filesize = filesize;
    this->dirty = true;
}

std::vector<dirty_block> ext2fs_inode::GetDataWrites() {
    std::vector<dirty_block> blocks{};
    for (int i = 0; i < blockCache.size(); i++) {
        auto block = blockCache[i];
        if (block && block->IsDirty()) {
            uint64_t startingAddr = i;
            startingAddr *= FILEPAGE_PAGE_SIZE;
            uint64_t endingAddr = startingAddr + block->GetDirtyLengthAndClear();
            uint64_t startingBlock = startingAddr / blocksize;
            uint64_t endingBlock = endingAddr / blocksize;
            if ((endingAddr % blocksize) == 0) {
                --endingBlock;
            }

            for (typeof(startingBlock) i = startingBlock; i <= endingBlock; i++) {
                auto addr = i * blocksize;
                uint32_t fileblock = (uint32_t) (addr / FILEPAGE_PAGE_SIZE);
                uint32_t fileblock_offset = (uint32_t) (addr % FILEPAGE_PAGE_SIZE);
                uint64_t blockaddr = blockRefs[i];
                blockaddr = blockaddr * blocksize;
                blockaddr /= bdev->GetBlocksize();
                dirty_block blk{.page1 = blockCache[fileblock], .page2 = {}, .blockaddr = blockaddr, .offset = fileblock_offset, .length = (uint32_t) blocksize};
                if ((fileblock_offset + blocksize) > FILEPAGE_PAGE_SIZE) {
                    blk.page2 = blockCache[fileblock + 1];
                }
                blocks.emplace_back(blk);
            }
        }
    }
    return blocks;
}

std::vector<dirty_block> ext2fs_inode::GetMetaWrites() {
    std::vector<dirty_block> inodeBlocks{};
    auto dirty = this->dirty;
    if (dirty) {
        ext2inode inode{};
        if (!Read(inode)) {
            return {};
        }
        {
            uint64_t i_blocks = blockRefs.size();
            i_blocks = i_blocks * blocksize;
            i_blocks = i_blocks / 512;
            inode.blocks = i_blocks;
        }
        inode.mode = mode;
        inode.uid = (uint16_t) (uid & 0xFFFF);
        inode.size = filesize;
        inode.atime = atime;
        inode.ctime = ctime;
        inode.mtime = mtime;
        inode.dtime = dtime;
        inode.gid = (uint16_t) (gid & 0xFFFF);
        inode.flags = flags;
        inode.reserved1 = 0;
        inode.generation = generation;
        inode.file_acl = file_acl;
        inode.dir_acl = dir_acl;
        inode.fragment_address = fragment_address;
        inode.fragment_number = fragment_number;
        inode.fragment_size = fragment_size;
        inode.uid_high = (uint16_t) ((uid & 0xFFFF0000) >> 16);
        inode.gid_high = (uint16_t) ((gid & 0xFFFF0000) >> 16);
        inode.links_count = linkCount;
        auto blockIterator = blockRefs.begin();
        {
            int i = 0;
            while (i < EXT2_NUM_DIRECT_BLOCK_PTRS && blockIterator != blockRefs.end()) {
                inode.block[i] = *blockIterator;
                ++blockIterator;
                ++i;
            }
            while (i < EXT2_NUM_DIRECT_BLOCK_PTRS) {
                inode.block[i] = 0;
                ++i;
            }
            while (i < EXT2_NUM_BLOCK_POINTERS) {
                inode.block[i] = 0;
                ++i;
            }
        }
        if (!Write(inode)) {
            return {};
        }
        auto wsize = WriteSize() < inodesize ? inodesize : WriteSize();
        dirty_block blk{.page1 = blocks[0], .page2 = blocks[1], .blockaddr = blocknum, .offset = 0, .length = (uint32_t) (GetOffset() + wsize)};
        inodeBlocks.emplace_back(blk);
        this->dirty = false;
    }
    return inodeBlocks;
}

std::vector<std::vector<dirty_block>> ext2fs_inode::GetWrites() {

    std::vector<std::vector<dirty_block>> writeGroups{};
    {
        auto blocks = GetDataWrites();
        if (!blocks.empty()) {
            writeGroups.push_back(blocks);
        }
    }
    auto inodeBlocks = GetMetaWrites();
    if (!inodeBlocks.empty()) {
        writeGroups.emplace_back(inodeBlocks);
    }
    return writeGroups;
}
