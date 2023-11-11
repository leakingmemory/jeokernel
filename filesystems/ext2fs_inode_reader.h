//
// Created by sigsegv on 10/5/23.
//

#ifndef FSBITS_EXT2FS_INODE_READER_H
#define FSBITS_EXT2FS_INODE_READER_H

#include "ext2fs_inode_results.h"
#include <memory>
#include <files/fsreferrer.h>
#include <files/fsreference.h>

class ext2fs_inode;

class ext2fs_inode_reader : public fsreferrer {
private:
    fsreference<ext2fs_inode> inode;
    std::weak_ptr<ext2fs_inode_reader> self_ref;
    std::shared_ptr<filepage_pointer> page;
    std::size_t offset, next_blki, cur_blki;
private:
    ext2fs_inode_reader();
    void Init(fsresource<ext2fs_inode> &inode);
    void Init(const fsreference<ext2fs_inode> &inode);
public:
    static std::shared_ptr<ext2fs_inode_reader> Create(fsresource<ext2fs_inode> &inode);
    static std::shared_ptr<ext2fs_inode_reader> Create(const fsreference<ext2fs_inode> &inode);
    std::string GetReferrerIdentifier() override;
    filesystem_status seek_set(std::size_t offset);
    inode_read_bytes_result read(void *ptr, std::size_t bytes);
    inode_read_bytes_result write(const void *ptr, std::size_t bytes);
};

#endif //FSBITS_EXT2FS_INODE_READER_H
