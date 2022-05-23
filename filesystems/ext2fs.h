//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_EXT2FS_H
#define FSBITS_EXT2FS_H

#include <memory>
#include <tuple>
#include <filesystems/filesystem.h>
#include <files/directory.h>

class blockdev;

struct ext2super;

class ext2fs_inode;

class ext2fs;
class ext2fs_provider;

class ext2inode;

class ext2fs_inode_table_block {
    friend ext2fs_inode;
    friend ext2fs;
private:
    uint8_t *data;
    uint32_t ref;
public:
    ext2fs_inode_table_block(std::size_t blocksize) : data(nullptr), ref(0) {
        data = (uint8_t *) malloc(blocksize);
    }
    ext2fs_inode_table_block(const ext2fs_inode_table_block &) = delete;
    ext2fs_inode_table_block(ext2fs_inode_table_block &&) = delete;
    ext2fs_inode_table_block &operator =(const ext2fs_inode_table_block &) = delete;
    ext2fs_inode_table_block &operator =(ext2fs_inode_table_block &&) = delete;
    ~ext2fs_inode_table_block() {
        if (data != nullptr) {
            free(data);
            data = nullptr;
        }
    }
};

class ext2fs_group {
    friend ext2fs;
private:
    std::vector<std::shared_ptr<ext2fs_inode_table_block>> InodeTableBlocks;
    std::size_t InodeTableBlock;
    std::size_t InodeTableOffset;
public:
    ext2fs_group() : InodeTableBlocks(), InodeTableBlock(0), InodeTableOffset(0) {}
};

class ext2fs_inode;

class ext2fs : public blockdev_filesystem {
    friend ext2fs_provider;
private:
    std::unique_ptr<ext2super> superblock;
    std::vector<ext2fs_group> groups;
    std::vector<std::tuple<uint32_t,std::shared_ptr<ext2fs_inode>>> inodes;
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
private:
    std::shared_ptr<ext2fs_inode> LoadInode(std::size_t inode_num);
public:
    std::shared_ptr<ext2fs_inode> GetInode(std::size_t inode_num);
    std::shared_ptr<directory> GetDirectory(std::size_t inode_num);
public:
    std::shared_ptr<directory> GetRootDirectory() override;
};

class ext2fs_inode {
    friend ext2fs;
private:
    std::shared_ptr<ext2fs_inode_table_block> blocks[2];
    std::size_t offset;
    std::size_t blocksize;
    std::vector<uint32_t> blockRefs;
public:
    ext2fs_inode(std::shared_ptr<ext2fs_inode_table_block> blk, std::size_t offset, std::size_t blocksize) : blocks(), offset(offset), blocksize(blocksize), blockRefs() {
        blocks[0] = blk;
        blk->ref++;
    }
    ext2fs_inode(std::shared_ptr<ext2fs_inode_table_block> blk1, std::shared_ptr<ext2fs_inode_table_block> blk2, std::size_t offset, std::size_t blocksize) : ext2fs_inode(blk1, offset, blocksize) {
        blocks[1] = blk2;
        blk2->ref++;
    }
    void Read(ext2inode &inode);
};

class ext2fs_provider : public filesystem_provider {
public:
    ext2fs_provider();
    std::string name() const override;
    std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev) const override;
};

#endif //FSBITS_EXT2FS_H
