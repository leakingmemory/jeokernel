//
// Created by sigsegv on 10/5/23.
//

#ifndef FSBITS_EXT2FS_INODE_READER_H
#define FSBITS_EXT2FS_INODE_READER_H

#include "ext2fs_inode_results.h"
#include <memory>

class ext2fs_inode;

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

#endif //FSBITS_EXT2FS_INODE_READER_H
