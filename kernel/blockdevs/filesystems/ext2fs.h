//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_EXT2FS_H
#define FSBITS_EXT2FS_H

#include <memory>
#include <tuple>
#include <mutex>
#include <filesystems/filesystem.h>
#include <files/directory.h>
#include <files/filepage.h>
#include <files/fsreference.h>
#include "ext2fs_inode_results.h"

class blockdev;

struct ext2super;

class blockdev_block;
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
    std::vector<std::shared_ptr<filepage>> InodeTableBlocks{};
    std::size_t InodeBitmapBlock{0};
    std::size_t InodeTableBlock{0};
    std::size_t InodeTableOffset{0};
    std::size_t BlockBitmapBlock{0};
    uint32_t freeInodesCount{0};
    uint32_t freeBlocksCount{0};
    uint32_t directoryCount{0};
    bool dirty{false};
public:
    ext2fs_group() = default;
};

class ext2fs_inode;

struct ext2fs_inode_with_id {
    uint32_t inode_num;
    std::shared_ptr<ext2fs_inode> inode;
};

struct ext2fs_get_inode_result {
    std::shared_ptr<ext2fs_inode> inode;
    filesystem_status status;
};

struct ext2fs_allocate_blocks_result {
    std::size_t block;
    std::size_t count;
    filesystem_status status;
};

class fsresourcelockfactory;
class ext2bitmap;
class ext2blockgroups;
class symlink;
class fsreferrer;

class ext2fs : public blockdev_filesystem {
    friend ext2fs_provider;
private:
    std::mutex mtx;
    std::weak_ptr<ext2fs> self_ref;
    std::shared_ptr<fsresourcelockfactory> fsreslockfactory;
    std::shared_ptr<blockdev_block> superblock_blocks;
    std::unique_ptr<ext2super> superblock;
    std::shared_ptr<blockdev_block> groups_blocks;
    std::shared_ptr<ext2blockgroups> RawBlockGroups;
    std::vector<ext2fs_group> groups;
    std::vector<ext2fs_inode_with_id> inodes;
    std::vector<std::shared_ptr<ext2bitmap>> blockBitmap{};
    std::vector<std::shared_ptr<ext2bitmap>> inodeBitmap{};
    uintptr_t sys_dev_id;
    std::size_t superblock_offset;
    uint32_t superblock_start, superblock_size;
    uint32_t BlockSize;
    uint32_t PhysBlockGroupsBlock, PhysBlockGroupsOffset, BlockGroupsTotalBlocks;
    bool filesystemWasValid, filesystemOpenForWrite;
public:
    ext2fs(std::shared_ptr<blockdev> bdev, const std::shared_ptr<fsresourcelockfactory> &fsreslockfactory);
    bool HasSuperblock() const;
    int VersionMajor() const ;
    int VersionMinor() const ;
    bool IsDynamic() const {
        return VersionMajor() >= 1;
    }
    uint16_t FsSignature() const;
    std::size_t InodeSize() const;
private:
    uint64_t FsBlockToPhysBlock(uint64_t fs_block);
    uint64_t FsBlockOffsetOnPhys(uint64_t fs_block);
    uint64_t FsBlocksToPhysBlocks(uint64_t fs_block, uint64_t fs_len);
    bool ReadBlockGroups();
private:
    ext2fs_get_inode_result LoadInode(std::size_t inode_num);
public:
    ext2fs_get_inode_result GetInode(std::size_t inode_num);
    filesystem_get_node_result<fsreference<directory>> GetDirectory(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer, std::size_t inode_num);
    filesystem_get_node_result<fsreference<fileitem>> GetFile(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer, std::size_t inode_num);
    filesystem_get_node_result<fsreference<symlink>> GetSymlink(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer, std::size_t inode_num);
    ext2fs_get_inode_result AllocateInode();
    filesystem_status ReleaseInode(uint32_t inodeNum);
    ext2fs_allocate_blocks_result AllocateBlocks(std::size_t requestedCount);
    filesystem_status ReleaseBlock(uint32_t blknum);
    void IncrementDirCount(uint32_t inodeNum);
private:
    std::vector<dirty_block> GetDataWritesForFlush();
    std::vector<dirty_block> GetMetaWrites();
    std::vector<dirty_block> GetBitmapWrites();
public:
    std::vector<std::vector<dirty_block>> GetWrites() override;
    std::vector<dirty_block> OpenForWrite() override;
    FlushOrCloseResult FlushOrClose() override;
public:
    filesystem_get_node_result<fsreference<directory>> GetRootDirectory(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer) override;
};

class ext2fs_file;
class ext2fs_symlink;
class ext2fs_directory;

class ext2fs_provider : public blockdev_filesystem_provider {
public:
    ext2fs_provider();
    std::string name() const override;
    std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev, const std::shared_ptr<fsresourcelockfactory> &fsreslockfactory) const override;
};

#endif //FSBITS_EXT2FS_H
