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
    std::vector<std::shared_ptr<ext2fs_inode_table_block>> InodeTableBlocks;
    std::size_t InodeTableBlock;
    std::size_t InodeTableOffset;
public:
    ext2fs_group() : InodeTableBlocks(), InodeTableBlock(0), InodeTableOffset(0) {}
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

class ext2fs : public blockdev_filesystem {
    friend ext2fs_provider;
private:
    std::mutex mtx;
    std::unique_ptr<ext2super> superblock;
    std::vector<ext2fs_group> groups;
    std::vector<ext2fs_inode_with_id> inodes;
    uintptr_t sys_dev_id;
    uint32_t BlockSize;
public:
    ext2fs(std::shared_ptr<blockdev> bdev);
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
    filesystem_get_node_result<directory> GetDirectory(std::shared_ptr<filesystem> shared_this, std::size_t inode_num);
    filesystem_get_node_result<fileitem> GetFile(std::shared_ptr<filesystem> shared_this, std::size_t inode_num);
    filesystem_get_node_result<fileitem> GetSymlink(std::shared_ptr<filesystem> shared_this, std::size_t inode_num);
public:
    filesystem_get_node_result<directory> GetRootDirectory(std::shared_ptr<filesystem> shared_this) override;
};

class ext2fs_file;
class ext2fs_symlink;

struct inode_read_blocks_result {
    std::shared_ptr<blockdev_block> block;
    filesystem_status status;
};

struct inode_read_blocks_raw_result {
    std::shared_ptr<filepage> page;
    filesystem_status status;
};

struct inode_read_block_result {
    std::shared_ptr<filepage_pointer> page;
    filesystem_status status;
};

struct inode_read_bytes_result {
    std::size_t size;
    filesystem_status status;
};

class ext2fs_inode {
    friend ext2fs;
    friend ext2fs_file;
    friend ext2fs_symlink;
private:
    std::mutex mtx;
    std::shared_ptr<blockdev> bdev;
    std::shared_ptr<ext2fs_inode_table_block> blocks[2];
    std::size_t offset;
    std::size_t blocksize;
    std::vector<uint32_t> blockRefs;
    std::string symlinkPointer;
    std::vector<std::shared_ptr<filepage>> blockCache;
    uintptr_t sys_dev_id;
    uintptr_t inode;
    uint64_t filesize;
    uint16_t mode;
public:
    ext2fs_inode(std::shared_ptr<blockdev> bdev, std::shared_ptr<ext2fs_inode_table_block> blk, std::size_t offset, std::size_t blocksize) : mtx(), bdev(bdev), blocks(), offset(offset), blocksize(blocksize), blockRefs(), blockCache(), inode(0), filesize(0), mode(0) {
        blocks[0] = blk;
        blk->ref++;
    }
    ext2fs_inode(std::shared_ptr<blockdev> bdev, std::shared_ptr<ext2fs_inode_table_block> blk1, std::shared_ptr<ext2fs_inode_table_block> blk2, std::size_t offset, std::size_t blocksize) : ext2fs_inode(bdev, blk1, offset, blocksize) {
        blocks[1] = blk2;
        blk2->ref++;
    }
    bool Read(ext2inode &inode);
private:
    inode_read_blocks_result ReadBlocks(uint32_t startingBlock, uint32_t startingOffset, uint32_t length);
    inode_read_blocks_raw_result ReadBlockRaw(std::size_t blki);
public:
    inode_read_block_result ReadBlock(std::size_t blki);
    inode_read_bytes_result ReadBytes(uint64_t offset, void *ptr, std::size_t length);
    uint64_t GetFileSize() {
        return filesize;
    }
};

class ext2fs_inode_reader {
private:
    std::shared_ptr<ext2fs_inode> inode;
    std::shared_ptr<filepage_pointer> page;
    std::size_t offset, blki;
public:
    ext2fs_inode_reader(std::shared_ptr<ext2fs_inode> inode) : inode(inode), page(), offset(0), blki(0) {}
    inode_read_bytes_result read(void *ptr, std::size_t bytes);
};

class ext2fs_provider : public blockdev_filesystem_provider {
public:
    ext2fs_provider();
    std::string name() const override;
    std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev) const override;
};

#endif //FSBITS_EXT2FS_H
