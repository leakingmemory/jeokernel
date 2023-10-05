//
// Created by sigsegv on 10/5/23.
//

#ifndef FSBITS_EXT2FS_INODE_RESULTS_H
#define FSBITS_EXT2FS_INODE_RESULTS_H

#include <memory>
#include <filesystems/filesystem.h>

class blockdev_block;

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

#endif //FSBITS_EXT2FS_INODE_RESULTS_H
