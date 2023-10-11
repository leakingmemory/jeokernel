//
// Created by sigsegv on 10/5/23.
//

#ifndef FSBITS_EXT2FS_INODE_H
#define FSBITS_EXT2FS_INODE_H

#include <mutex>
#include <memory>
#include <vector>
#include <filesystems/filesystem.h>
#include <files/filepage.h>

#include "ext2fs_inode_results.h"

class blockdev;
class blockdev_block;

class ext2fs;
class ext2fs_file;
class ext2fs_symlink;
class ext2fs_directory;

class ext2inode;

class ext2fs_inode {
    friend ext2fs;
    friend ext2fs_file;
    friend ext2fs_symlink;
    friend ext2fs_directory;
private:
    std::mutex mtx;
    std::weak_ptr<ext2fs> fs;
    std::shared_ptr<blockdev> bdev;
    std::shared_ptr<filepage> blocks[2];
    std::size_t offset;
    std::size_t blocksize;

    std::vector<uint32_t> blockRefs;
    std::string symlinkPointer;
    std::vector<std::shared_ptr<filepage>> blockCache;
    uintptr_t sys_dev_id;
    uintptr_t inode;
    uint64_t filesize;
    uint64_t blocknum;
    uint16_t mode;
    uint16_t linkCount;
    bool dirty{false};
public:
    ext2fs_inode(const std::weak_ptr<ext2fs> &fs, const std::shared_ptr<blockdev> &bdev, const std::shared_ptr<filepage> &blk, std::size_t offset, std::size_t blocksize, uint64_t blocknum) : mtx(), fs(fs), bdev(bdev), blocks(), offset(offset), blocksize(blocksize), blockRefs(), symlinkPointer(), blockCache(), sys_dev_id(), inode(0), filesize(0), blocknum(blocknum), mode(0) {
        blocks[0] = blk;
    }
    ext2fs_inode(const std::weak_ptr<ext2fs> &fs, const std::shared_ptr<blockdev> &bdev, const std::shared_ptr<filepage> &blk1, std::shared_ptr<filepage> blk2, std::size_t offset, std::size_t blocksize, uint64_t blocknum) : ext2fs_inode(fs, bdev, blk1, offset, blocksize, blocknum) {
        blocks[1] = blk2;
    }
    bool Init();
    bool Read(ext2inode &inode);
    bool Write(ext2inode &inode);
    size_t GetOffset() const {
        return offset;
    }
    size_t WriteSize() const;
private:
    inode_read_blocks_result ReadBlocks(uint32_t startingBlock, uint32_t startingOffset, uint32_t length);
public:
    filesystem_status AllocToExtend(std::size_t blki, uint32_t sizeAlloc = 0);
private:
    inode_read_blocks_raw_result ReadBlockRaw(std::size_t blki, bool alloc = false, uint32_t sizeAlloc = 0);
public:
    inode_read_block_result ReadBlock(std::size_t blki);

    inode_read_block_result ReadOrAllocBlock(std::size_t blki, std::size_t sizeAlloc);
    inode_read_bytes_result ReadBytes(uint64_t offset, void *ptr, std::size_t length);
    inode_read_bytes_result WriteBytes(uint64_t offset, void *ptr, std::size_t length);
    uint64_t GetFileSize() {
        return filesize;
    }
    void SetFileSize(uint64_t filesize);
    std::vector<dirty_block> GetDataWrites();
    std::vector<std::vector<dirty_block>> GetWrites();
};

#endif //FSBITS_EXT2FS_INODE_H
