//
// Created by sigsegv on 4/25/22.
//

#include "ext2fs.h"
#include <blockdevs/blockdev.h>
#include <cstring>
#include <iostream>
#include "ext2fs/ext2struct.h"

ext2fs::ext2fs(std::shared_ptr<blockdev> bdev) : blockdev_filesystem(bdev), groups() {
    auto blocksize = bdev->GetBlocksize();
    {
        std::shared_ptr<blockdev_block> blocks;
        std::size_t superblock_offset;
        {
            auto superblock_start = 1024 / blocksize;
            {
                auto start_offset = superblock_start * blocksize;
                superblock_offset = (std::size_t) (1024 - start_offset);
            }
            auto superblock_size = (2048 / blocksize) - superblock_start;
            if (superblock_size < 1) {
                superblock_size = 1;
            }
            blocks = bdev->ReadBlock(superblock_start, superblock_size);
        }
        superblock = std::make_unique<ext2super>();
        memmove(&(*superblock), ((uint8_t *) blocks->Pointer()) + superblock_offset, sizeof(*superblock));
        BlockSize = 1024 << superblock->log2_blocksize_minus_10;
    }
}

bool ext2fs::HasSuperblock() {
    if (superblock) {
        return true;
    } else {
        return false;
    }
}

int ext2fs::VersionMajor() {
    return superblock->major_version;
}

int ext2fs::VersionMinor() {
    return superblock->minor_version;
}

uint16_t ext2fs::FsSignature() {
    return superblock->ext2_signature;
}

uint64_t ext2fs::FsBlockToPhysBlock(uint64_t fs_block) {
    fs_block *= BlockSize;
    return fs_block / bdev->GetBlocksize();
}

uint64_t ext2fs::FsBlockOffsetOnPhys(uint64_t fs_block) {
    fs_block *= BlockSize;
    return fs_block % bdev->GetBlocksize();
}

uint64_t ext2fs::FsBlocksToPhysBlocks(uint64_t fs_block, uint64_t fs_len) {
    auto start = FsBlockToPhysBlock(fs_block);
    auto fs_end = fs_block + fs_len;
    auto end = FsBlockToPhysBlock(fs_end);
    if (FsBlockOffsetOnPhys(fs_end) != 0) {
        ++end;
    }
    return end - start;
}

bool ext2fs::ReadBlockGroups() {
    uint32_t blockGroups = superblock->total_inodes / superblock->inodes_per_group;
    if ((superblock->total_inodes % superblock->inodes_per_group) != 0) {
        ++blockGroups;
    }
    std::cout << "Total inodes of " << superblock->total_inodes << " and per group " << superblock->inodes_per_group
              << " gives block groups " << blockGroups << "\n";
    auto superblockBlock = 1024 / BlockSize;
    uint64_t blockGroupsBlock = superblockBlock + 1;
    uint64_t onDiskBlockGRoups = FsBlockToPhysBlock(blockGroupsBlock);
    uint64_t onDiskBlockGroupsOffset = FsBlockOffsetOnPhys(blockGroupsBlock);
    std::cout << "Superblock in block " << superblockBlock << " gives block groups in " << blockGroupsBlock << " on disk " << onDiskBlockGRoups << "\n";
    uint32_t blockGroupsSize = onDiskBlockGroupsOffset + (blockGroups * sizeof(ext2blockgroup));
    uint32_t blockGroupsTotalBlocks = blockGroupsSize / bdev->GetBlocksize();
    if ((blockGroupsSize % bdev->GetBlocksize()) != 0) {
        ++blockGroupsTotalBlocks;
    }
    std::cout << "Block groups size " << blockGroupsSize << " phys blocks " << blockGroupsTotalBlocks << "\n";
    auto physBlockGroups = bdev->ReadBlock(onDiskBlockGRoups, blockGroupsTotalBlocks);
    if (physBlockGroups) {
        groups.reserve(blockGroups);
        std::shared_ptr<ext2blockgroups> groups{new ext2blockgroups(blockGroups)};
        memcpy(&((*groups)[0]), ((uint8_t *) physBlockGroups->Pointer()) + onDiskBlockGroupsOffset, sizeof((*groups)[0]) * blockGroups);
        for (std::size_t i = 0; i < blockGroups; i++) {
            auto &groupObject = this->groups.emplace_back();
            auto &group = (*groups)[i];
            std::cout << "Block group " << group.free_blocks_count << " free block, " << group.free_inodes_count
            << " free inodes, " << group.block_bitmap << "/" << group.inode_bitmap << " block/inode bitmaps, "
            << group.inode_table << " inode table, " << group.used_dirs_count << " dirs.\n";
            auto blockBitmapBlock = FsBlockToPhysBlock(group.block_bitmap);
            auto blockBitmapOffset = FsBlockOffsetOnPhys(group.block_bitmap);
            auto blockBitmapPhysLength = FsBlocksToPhysBlocks(group.block_bitmap, 1);
            auto blockBitmapBlocks = bdev->ReadBlock(blockBitmapBlock, blockBitmapPhysLength);
            if (blockBitmapBlocks) {
                std::shared_ptr<ext2bitmap> blockBitmap{new ext2bitmap(superblock->blocks_per_group)};
                {
                    auto size = superblock->blocks_per_group >> 3;
                    if ((superblock->blocks_per_group & 7) != 0) {
                        ++size;
                    }
                    memcpy(blockBitmap->Pointer(), ((uint8_t *) blockBitmapBlocks->Pointer()) + blockBitmapOffset, size);
                }
                int free_count = 0;
                for (int i = 0; i < superblock->blocks_per_group; i++) {
                    if (!(*blockBitmap)[i]) {
                        ++free_count;
                    }
                }
                std::cout << "Found " << free_count << " free blocks at " << blockBitmapBlock << " offset "
                << blockBitmapOffset << " and num " << blockBitmapPhysLength << "\n";
            } else {
                std::cerr << "Error reading block group block bitmap\n";
                return false;
            }
            auto inodeBitmapBlock = FsBlockToPhysBlock(group.inode_bitmap);
            auto inodeBitmapOffset = FsBlockOffsetOnPhys(group.inode_bitmap);
            auto inodeBitmapPhysLength = FsBlocksToPhysBlocks(group.inode_bitmap, 1);
            auto inodeBitmapBlocks = bdev->ReadBlock(inodeBitmapBlock, inodeBitmapPhysLength);
            if (inodeBitmapBlocks) {
                std::shared_ptr<ext2bitmap> inodeBitmap{new ext2bitmap(superblock->inodes_per_group)};
                {
                    auto size = superblock->inodes_per_group >> 3;
                    if ((superblock->inodes_per_group & 7) != 0) {
                        ++size;
                    }
                    memcpy(inodeBitmap->Pointer(), ((uint8_t *) inodeBitmapBlocks->Pointer()) + inodeBitmapOffset, size);
                }
                int free_count = 0;
                for (int i = 0; i < superblock->inodes_per_group; i++) {
                    if (!(*inodeBitmap)[i]) {
                        ++free_count;
                    }
                }
                std::cout << "Found " << free_count << " free inodes at " << inodeBitmapBlock << " offset "
                          << inodeBitmapOffset << " and num " << inodeBitmapPhysLength << "\n";
            } else {
                std::cerr << "Error reading block group inode bitmap\n";
                return false;
            }
            groupObject.InodeTableBlock = FsBlockToPhysBlock(group.inode_table);
            groupObject.InodeTableOffset = FsBlockOffsetOnPhys(group.inode_table);
            auto inodeTableSize = superblock->inodes_per_group * sizeof(ext2inode);
            auto inodeTablePhysBlocks = (inodeTableSize + groupObject.InodeTableOffset) / bdev->GetBlocksize();
            std::cout << "Inode table starts at " << groupObject.InodeTableBlock << " offset " << groupObject.InodeTableOffset
            << " size of " << inodeTableSize << " and blocks " << inodeTablePhysBlocks << "\n";
            groupObject.InodeTableBlocks.reserve(inodeTablePhysBlocks);
            for (std::size_t i = 0; i < inodeTablePhysBlocks; i++) {
                groupObject.InodeTableBlocks.emplace_back();
            }
        }
        return true;
    } else {
        std::cerr << "Error reading block groups for ext2fs\n";
        return false;
    }
}

std::shared_ptr<ext2fs_inode> ext2fs::LoadInode(std::size_t inode_num) {
    --inode_num;
    auto group_idx = inode_num / superblock->inodes_per_group;
    auto inode_off = inode_num % superblock->inodes_per_group;
    if (group_idx >= groups.size()) {
        return {};
    }
    auto &group = groups[group_idx];
    auto offset = inode_off * sizeof(ext2inode);
    offset += group.InodeTableOffset;
    auto end = offset + sizeof(ext2inode) - 1;
    auto block_num = offset / bdev->GetBlocksize();
    offset = offset % bdev->GetBlocksize();
    auto block_end = end / bdev->GetBlocksize();
    std::cout << "Inode " << (inode_num + 1) << " at " << block_num << " - " << block_end << " off "
    << offset << "\n";
    if (!group.InodeTableBlocks[block_num]) {
        auto rd = bdev->ReadBlock(block_num + group.InodeTableBlock, 1);
        if (rd && !group.InodeTableBlocks[block_num]) {
            group.InodeTableBlocks[block_num] = std::make_shared<ext2fs_inode_table_block>(bdev->GetBlocksize());
            memcpy(&(group.InodeTableBlocks[block_num]->data[0]), rd->Pointer(), bdev->GetBlocksize());
        }
    }
    std::shared_ptr<ext2fs_inode> inode_obj{};
    if (block_num == block_end) {
        inode_obj = std::make_shared<ext2fs_inode>(bdev, group.InodeTableBlocks[block_num], offset, BlockSize);
    } else if ((block_num + 1) == block_end) {
        inode_obj = std::make_shared<ext2fs_inode>(bdev, group.InodeTableBlocks[block_num], group.InodeTableBlocks[block_end], offset, BlockSize);
    } else {
        return {};
    }
    ext2inode inode{};
    inode_obj->Read(inode);
    std::cout << "Inode " << inode_num << " " << std::oct << inode.mode << std::dec
              << " sz " << inode.size << " blks " << inode.blocks << ":";
    for (int i = 0; i < EXT2_NUM_DIRECT_BLOCK_PTRS; i++) {
        if (inode.block[i]) {
            std::cout << " " << inode.block[i];
            inode_obj->blockRefs.push_back(inode.block[i]);
        }
    }
    inode_obj->filesize = inode.size;
    inode_obj->mode = inode.mode;
    auto pages = inode_obj->filesize / FILEPAGE_PAGE_SIZE;
    if ((inode_obj->filesize % FILEPAGE_PAGE_SIZE) != 0) {
        ++pages;
    }
    inode_obj->blockCache.reserve(pages);
    for (int i = 0; i < pages; i++) {
        inode_obj->blockCache.push_back(0);
    }
    std::cout << "\n";
    return inode_obj;
}

std::shared_ptr<ext2fs_inode> ext2fs::GetInode(std::size_t inode_num) {
    for (auto &inode : inodes) {
        if (std::get<0>(inode) == inode_num) {
            return std::get<1>(inode);
        }
    }
    auto loadedInode = LoadInode(inode_num);
    if (loadedInode) {
        for (auto &inode : inodes) {
            if (std::get<0>(inode) == inode_num) {
                return std::get<1>(inode);
            }
        }
        std::tuple<uint32_t,std::shared_ptr<ext2fs_inode>> tuple = std::make_tuple((uint32_t) inode_num, loadedInode);
        inodes.push_back(tuple);
        return std::get<1>(tuple);
    }
    return {};
}

class ext2fs_file : public fileitem {
protected:
    std::shared_ptr<filesystem> fs;
    std::shared_ptr<ext2fs_inode> inode;
public:
    ext2fs_file(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode);
    uint32_t Mode() override;
    std::size_t Size() override;
};

ext2fs_file::ext2fs_file(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode) : fs(fs), inode(inode) {
}

uint32_t ext2fs_file::Mode() {
    return inode->mode;
}

std::size_t ext2fs_file::Size() {
    return inode->filesize;
}

class ext2fs_directory : public directory, public ext2fs_file {
private:
    std::vector<std::shared_ptr<directory_entry>> entries;
    bool entriesRead;
public:
    ext2fs_directory(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode) : directory(), ext2fs_file(fs, inode), entries(), entriesRead(false) {}
    std::vector<std::shared_ptr<directory_entry>> Entries() override;
private:
    ext2fs &Filesystem() {
        filesystem *fs = &(*(this->fs));
        return *((ext2fs *) fs);
    }
    uint32_t Mode() override;
    std::size_t Size() override;
};

std::vector<std::shared_ptr<directory_entry>> ext2fs_directory::Entries() {
    if (entriesRead) {
        return entries;
    }
    std::vector<std::shared_ptr<directory_entry>> entries{};
    ext2fs_inode_reader reader{inode};
    {
        ext2dirent baseDirent{};
        do {
            if (reader.read(&baseDirent, sizeof(baseDirent)) == sizeof(baseDirent) &&
                baseDirent.rec_len > sizeof(baseDirent) &&
                baseDirent.name_len > 0) {
                auto addLength = baseDirent.rec_len - sizeof(baseDirent);
                auto *dirent = new (malloc(baseDirent.rec_len)) ext2dirent();
                memcpy(dirent, &baseDirent, sizeof(*dirent));
                static_assert(sizeof(*dirent) == sizeof(baseDirent));
                if (reader.read(((uint8_t *) dirent) + sizeof(baseDirent), addLength) == addLength) {
                    if (dirent->inode != 0) {
                        std::string name{dirent->Name()};
                        std::cout << "Dir node " << dirent->inode << " type " << ((int) dirent->file_type) << " name "
                                  << name << "\n";
                        if (dirent->file_type == 2) {
                            std::shared_ptr<directory> dir{Filesystem().GetDirectory(fs, dirent->inode)};
                            entries.emplace_back(new directory_entry(name, dir));
                        } else if (dirent->file_type == 1) {
                            std::shared_ptr<fileitem> file{Filesystem().GetFile(fs, dirent->inode)};
                            entries.emplace_back(new directory_entry(name, file));
                        }
                    }
                } else {
                    baseDirent.inode = 0;
                }
                dirent->~ext2dirent();
                free(dirent);
            } else {
                baseDirent.inode = 0;
            }
        } while (baseDirent.inode != 0);
    }
    if (!entriesRead) {
        this->entries = entries;
        entriesRead = true;
    }
    return this->entries;
}

uint32_t ext2fs_directory::Mode() {
    return ext2fs_file::Mode();
}

std::size_t ext2fs_directory::Size() {
    return ext2fs_file::Size();
}

std::shared_ptr<directory> ext2fs::GetDirectory(std::shared_ptr<filesystem> shared_this, std::size_t inode_num) {
    if (!shared_this || this != ((ext2fs *) &(*shared_this))) {
        std::cerr << "Wrong shared reference for filesystem when opening directory\n";
        return {};
    }
    auto inode_obj = GetInode(inode_num);
    if (!inode_obj) {
        std::cerr << "Failed to open inode " << inode_num << "\n";
        return {};
    }
    return std::make_shared<ext2fs_directory>(shared_this, inode_obj);
}

std::shared_ptr<fileitem> ext2fs::GetFile(std::shared_ptr<filesystem> shared_this, std::size_t inode_num) {
    if (!shared_this || this != ((ext2fs *) &(*shared_this))) {
        std::cerr << "Wrong shared reference for filesystem when opening directory\n";
        return {};
    }
    auto inode_obj = GetInode(inode_num);
    if (!inode_obj) {
        std::cerr << "Failed to open inode " << inode_num << "\n";
        return {};
    }
    return std::make_shared<ext2fs_file>(shared_this, inode_obj);
}

std::shared_ptr<directory> ext2fs::GetRootDirectory(std::shared_ptr<filesystem> shared_this) {
    return GetDirectory(shared_this, 2);
}

void ext2fs_inode::Read(ext2inode &inode) {
    auto blocksize = bdev->GetBlocksize();
    auto sz1 = sizeof(inode);
    if ((sz1 + offset) > blocksize) {
        sz1 = blocksize - offset;
        auto sz2 = sizeof(inode) - sz1;
        memcpy(((uint8_t *) (void *) &inode) + sz1, blocks[1]->data, sz2);
    }
    memcpy(&inode, ((uint8_t *) blocks[0]->data) + offset, sz1);
}

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

std::shared_ptr<blockdev_block> ext2fs_inode::ReadBlocks(uint32_t startingBlock, uint32_t startingOffset, uint32_t length) {
    std::vector<std::shared_ptr<blockdev_block>> readBlocks{};
    int hole = 0;
    while (startingOffset >= blocksize) {
        ++startingBlock;
        startingOffset -= blocksize;
    }
    uint32_t numBlocks = startingOffset + length;
    uint32_t lastBlockLength = numBlocks % blocksize;
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
                std::cout << "Read file block " << blockNo << ": phys " << physBlock << " num " << physBlocks << "\n";
                auto rdBlock = bdev->ReadBlock(physBlock, physBlocks);
                if (rdBlock && offset != 0) {
                    memcpy(rdBlock->Pointer(), ((uint8_t *) rdBlock->Pointer()) + offset, blocksize);
                }
                if (!rdBlock) {
                    std::cerr << "Read error file block " << blockNo << "\n";
                }
                readBlocks.push_back(rdBlock);
            } else {
                ++hole;
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
            std::cout << "Assemble blocks 0 - " << cp << "\n";
            memcpy(finalBlock->Pointer(), readBlocks[i]->Pointer(), cp);
        }
        offset = firstReadLength;
        ++i;
    }
    if (readBlocks.size() > 1) {
        while (i < (readBlocks.size() - 1)) {
            if (readBlocks[i]) {
                std::cout << "Assemble blocks " << offset << " - " << (offset + blocksize) << "\n";
                memcpy(((uint8_t *) finalBlock->Pointer()) + offset, readBlocks[i]->Pointer(), blocksize);
            }
            offset += blocksize;
            ++i;
        }
        if (i < readBlocks.size()) {
            if (readBlocks[i]) {
                std::cout << "Assemble blocks " << offset << " - " << (offset + lastBlockLength) << "\n";
                memcpy(((uint8_t *) finalBlock->Pointer()) + offset, readBlocks[i]->Pointer(), lastBlockLength);
            }
        }
    }
    return finalBlock;
}

std::shared_ptr<filepage_pointer> ext2fs_inode::ReadBlock(std::size_t blki) {
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
            auto readBlocks = ReadBlocks(startingBlock, readingOffset, FILEPAGE_PAGE_SIZE);
            if (readBlocks) {
                std::shared_ptr<filepage> page{new filepage()};
                memcpy(page->Pointer()->Pointer(), readBlocks->Pointer(), FILEPAGE_PAGE_SIZE);
                if (!blockCache[blki]) {
                    blockCache[blki] = page;
                }
                return blockCache[blki]->Pointer();
            } else {
                std::cerr << "Read error for inode block " << startingBlock << "\n";
                return {};
            }
        }
        return blockCache[blki]->Pointer();
    }
    return {};
}

ext2fs_provider::ext2fs_provider() {
}

std::string ext2fs_provider::name() const {
    return "ext2fs";
}

std::shared_ptr<blockdev_filesystem> ext2fs_provider::open(std::shared_ptr<blockdev> bdev) const {
    std::shared_ptr<ext2fs> fs{new ext2fs(bdev)};
    if (fs->HasSuperblock() && fs->FsSignature() == EXT2_SIGNATURE) {
        std::cout << "Ext2 signature " << fs->FsSignature() << " " << fs->VersionMajor() << "." << fs->VersionMinor() << "\n";
        if (!fs->ReadBlockGroups()) {
            return {};
        }
        return fs;
    } else {
        return {};
    }
}

std::size_t ext2fs_inode_reader::read(void *ptr, std::size_t bytes) {
    if (page) {
        auto remaining = FILEPAGE_PAGE_SIZE - offset;
        if (bytes <= remaining) {
            memcpy(ptr, ((uint8_t *) page->Pointer()) + offset, bytes);
            offset += bytes;
            if (offset == FILEPAGE_PAGE_SIZE) {
                page = {};
                ++blki;
                offset = 0;
            }
            return bytes;
        } else {
            memcpy(ptr, ((uint8_t *) page->Pointer()) + offset, remaining);
            page = {};
            ++blki;
            offset = 0;
            auto nextrd = read(((uint8_t *) ptr) + remaining, bytes - remaining);
            return remaining + nextrd;
        }
    } else {
        page = inode->ReadBlock(blki);
        ++blki;
        if (!page) {
            return 0;
        }
        return read(ptr, bytes);
    }
}