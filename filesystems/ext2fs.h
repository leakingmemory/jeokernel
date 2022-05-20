//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_EXT2FS_H
#define FSBITS_EXT2FS_H

#include <memory>
#include <filesystems/filesystem.h>

class blockdev;

struct ext2super;

class ext2fs_provider;

class ext2fs : public blockdev_filesystem {
    friend ext2fs_provider;
private:
    std::unique_ptr<ext2super> superblock;
    uint32_t BlockSize;
public:
    ext2fs(std::shared_ptr<blockdev> bdev);
    bool HasSuperblock();
    int VersionMajor();
    int VersionMinor();
    uint16_t FsSignature();
private:
    uint64_t FsBlockToPhysBlock(uint64_t fs_block);
    uint64_t FsBlockOffsetOnPhys(uint64_t fs_block);
    uint64_t FsBlocksToPhysBlocks(uint64_t fs_block, uint64_t fs_len);
    bool ReadBlockGroups();
};

class ext2fs_provider : public filesystem_provider {
public:
    ext2fs_provider();
    std::string name() const override;
    std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev) const override;
};

#endif //FSBITS_EXT2FS_H
