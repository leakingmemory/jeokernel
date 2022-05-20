//
// Created by sigsegv on 4/25/22.
//

#include "ext2fs.h"
#include <blockdevs/blockdev.h>
#include <cstring>
#include <iostream>
#include "ext2fs/ext2struct.h"

ext2fs::ext2fs(std::shared_ptr<blockdev> bdev) : blockdev_filesystem(bdev) {
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
        std::shared_ptr<ext2blockgroups> groups{new ext2blockgroups(blockGroups)};
        memcpy(&((*groups)[0]), ((uint8_t *) physBlockGroups->Pointer()) + onDiskBlockGroupsOffset, sizeof((*groups)[0]) * blockGroups);
        for (std::size_t i = 0; i < blockGroups; i++) {
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
        }
        return true;
    } else {
        std::cerr << "Error reading block groups for ext2fs\n";
        return false;
    }
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