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
    std::vector<std::shared_ptr<filepage>> InodeTableBlocks;
    std::size_t InodeBitmapBlock;
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

struct ext2fs_allocate_blocks_result {
    std::size_t block;
    std::size_t count;
    filesystem_status status;
};

class ext2bitmap;

class ext2fs : public blockdev_filesystem {
    friend ext2fs_provider;
private:
    std::mutex mtx;
    std::weak_ptr<ext2fs> self_ref;
    std::unique_ptr<ext2super> superblock;
    std::vector<ext2fs_group> groups;
    std::vector<ext2fs_inode_with_id> inodes;
    std::vector<std::shared_ptr<ext2bitmap>> blockBitmap{};
    std::vector<std::shared_ptr<ext2bitmap>> inodeBitmap{};
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
    ext2fs_get_inode_result AllocateInode();
    ext2fs_allocate_blocks_result AllocateBlocks(std::size_t requestedCount);
    filesystem_status ReleaseBlock(uint32_t blknum);
    std::vector<std::vector<dirty_block>> GetWrites() override;
public:
    filesystem_get_node_result<directory> GetRootDirectory(std::shared_ptr<filesystem> shared_this) override;
};

class ext2fs_file;
class ext2fs_symlink;
class ext2fs_directory;

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
    std::vector<std::vector<dirty_block>> GetWrites();
};

class ext2fs_inode_reader {
private:
    std::shared_ptr<ext2fs_inode> inode;
    std::shared_ptr<filepage_pointer> page;
    std::size_t offset, next_blki, cur_blki;
public:
    ext2fs_inode_reader(std::shared_ptr<ext2fs_inode> inode) : inode(inode), page(), offset(0), next_blki(0), cur_blki(0) {}
    filesystem_status seek_set(std::size_t offset);
    inode_read_bytes_result read(void *ptr, std::size_t bytes);
    inode_read_bytes_result write(const void *ptr, std::size_t bytes);
};

class ext2fs_provider : public blockdev_filesystem_provider {
public:
    ext2fs_provider();
    std::string name() const override;
    std::shared_ptr<blockdev_filesystem> open(std::shared_ptr<blockdev> bdev) const override;
};

#endif //FSBITS_EXT2FS_H
