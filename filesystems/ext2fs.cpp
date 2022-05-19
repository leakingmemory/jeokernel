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

ext2fs_provider::ext2fs_provider() {
}

std::string ext2fs_provider::name() const {
    return "ext2fs";
}

std::shared_ptr<blockdev_filesystem> ext2fs_provider::open(std::shared_ptr<blockdev> bdev) const {
    std::shared_ptr<ext2fs> fs{new ext2fs(bdev)};
    if (fs->HasSuperblock()) {
        std::cout << "Ext2 signature " << fs->FsSignature() << " " << fs->VersionMajor() << "." << fs->VersionMinor() << "\n";
        return fs;
    } else {
        return {};
    }
}