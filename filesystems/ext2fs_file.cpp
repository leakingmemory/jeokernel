//
// Created by sigsegv on 10/5/23.
//

#include "ext2fs_file.h"
#include "ext2fs_inode.h"
#include <files/fsresource.h>

ext2fs_file *ext2fs_file::GetResource() {
    return this;
}

ext2fs_file::ext2fs_file(std::shared_ptr<ext2fs> fs, fsresourcelockfactory &lockfactory) : fsresource<ext2fs_file>(lockfactory), fsreferrer("ext2fs_file"), fs(fs), inode() {
}

void ext2fs_file::Init(const std::shared_ptr<ext2fs_file> &self_ref, fsresource<ext2fs_inode> &inode) {
    SetSelfRef(self_ref);
    this->inode = inode.CreateReference(self_ref);
}

std::string ext2fs_file::GetReferrerIdentifier() {
    return "";
}

uint32_t ext2fs_file::Mode() {
    return inode->mode;
}

std::size_t ext2fs_file::Size() {
    return inode->filesize;
}

uintptr_t ext2fs_file::InodeNum() {
    return inode->inode;
}

uint32_t ext2fs_file::BlockSize() {
    return inode->blocksize;
}

uintptr_t ext2fs_file::SysDevId() {
    return inode->sys_dev_id;
}

file_getpage_result ext2fs_file::GetPage(std::size_t pagenum) {
    auto result = inode->ReadBlockRaw(pagenum);
    return {.page = result.page, .status = Convert(result.status)};
}

file_read_result ext2fs_file::Read(uint64_t offset, void *ptr, std::size_t length) {
    auto result = inode->ReadBytes(offset, ptr, length);
    return {.size = result.size, .status = Convert(result.status)};
}
