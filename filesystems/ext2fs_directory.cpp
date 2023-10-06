//
// Created by sigsegv on 10/5/23.
//

#include "ext2fs_directory.h"
#include "ext2fs_inode_reader.h"
#include "ext2fs_inode.h"
#include "ext2fs/ext2struct.h"
#include "ext2fs.h"
#include <filesystems/filesystem.h>
#include <cstring>

entries_result ext2fs_directory::Entries() {
    if (entriesRead) {
        return {.entries = entries, .status = fileitem_status::SUCCESS};
    }
    std::vector<std::shared_ptr<directory_entry>> entries{};
    ext2fs_inode_reader reader{inode};
    {
        ext2dirent baseDirent{};
        auto sizeRemaining = inode->GetFileSize();
        do {
            inode_read_bytes_result readResult{};
            if (sizeRemaining >= sizeof(baseDirent)) {
                readResult = reader.read(&baseDirent, sizeof(baseDirent));
            } else {
                readResult = {.size = 0, .status = filesystem_status::SUCCESS};
            }
            sizeRemaining -= readResult.size;
            if (readResult.size == sizeof(baseDirent) &&
                baseDirent.rec_len > sizeof(baseDirent) &&
                baseDirent.name_len > 0) {
                auto addLength = baseDirent.rec_len - sizeof(baseDirent);
                auto *dirent = new (malloc(baseDirent.rec_len)) ext2dirent();
                memcpy(dirent, &baseDirent, sizeof(*dirent));
                static_assert(sizeof(*dirent) == sizeof(baseDirent));
                if (sizeRemaining >= addLength) {
                    readResult = reader.read(((uint8_t *) dirent) + sizeof(baseDirent), addLength);
                    sizeRemaining -= readResult.size;
                } else {
                    readResult = {.size = 0, .status = filesystem_status::SUCCESS};
                }
                if (readResult.size == addLength) {
                    if (dirent->inode != 0) {
                        std::string name{dirent->Name()};
#ifdef DEBUG_DIR
                        std::cout << "Dir node " << dirent->inode << " type " << ((int) dirent->file_type) << " name "
                                  << name << "\n";
#endif
                        if (dirent->file_type == Ext2FileType_Directory) {
                            auto result = Filesystem().GetDirectory(fs, dirent->inode);
                            if (result.node) {
                                entries.emplace_back(new directory_entry(name, result.node));
                            } else {
                                return {.entries = {}, .status = Convert(result.status)};
                            }
                        } else if (dirent->file_type == Ext2FileType_Regular) {
                            auto result = Filesystem().GetFile(fs, dirent->inode);
                            if (result.node) {
                                entries.emplace_back(new directory_entry(name, result.node));
                            } else {
                                return {.entries = {}, .status = Convert(result.status)};
                            }
                        } else if (dirent->file_type == Ext2FileType_Symlink) {
                            auto result = Filesystem().GetSymlink(fs, dirent->inode);
                            if (result.node) {
                                entries.emplace_back(new directory_entry(name, result.node));
                            } else {
                                return {.entries = {}, .status = Convert(result.status)};
                            }
                        }
                    }
                } else {
                    if (readResult.status != filesystem_status::SUCCESS) {
                        return {.entries = {}, .status = Convert(readResult.status)};
                    }
                    baseDirent.inode = 0;
                }
                dirent->~ext2dirent();
                free(dirent);
            } else {
                if (readResult.status != filesystem_status::SUCCESS) {
                    return {.entries = {}, .status = Convert(readResult.status)};
                }
                baseDirent.inode = 0;
            }
        } while (baseDirent.inode != 0);
        actualSize = inode->GetFileSize() - sizeRemaining;
    }
    if (!entriesRead) {
        this->entries = entries;
        entriesRead = true;
    }
    return {.entries = this->entries, .status = fileitem_status::SUCCESS};
}

uint32_t ext2fs_directory::Mode() {
    return ext2fs_file::Mode();
}

std::size_t ext2fs_directory::Size() {
    return ext2fs_file::Size();
}

uintptr_t ext2fs_directory::InodeNum() {
    return ext2fs_file::InodeNum();
}

uint32_t ext2fs_directory::BlockSize() {
    return ext2fs_file::BlockSize();
}

uintptr_t ext2fs_directory::SysDevId() {
    return ext2fs_file::SysDevId();
}

file_getpage_result ext2fs_directory::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::INVALID_REQUEST};
}

file_read_result ext2fs_directory::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::INVALID_REQUEST};
}

directory_resolve_result ext2fs_directory::Create(std::string filename) {
    auto allocInode = Filesystem().AllocateInode();
    if (allocInode.status != filesystem_status::SUCCESS) {
        fileitem_status status;
        switch (allocInode.status) {
            case filesystem_status::IO_ERROR:
                status = fileitem_status::IO_ERROR;
                break;
            case filesystem_status::INTEGRITY_ERROR:
                status = fileitem_status::INTEGRITY_ERROR;
                break;
            case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
                status = fileitem_status::NOT_SUPPORTED_FS_FEATURE;
                break;
            case filesystem_status::INVALID_REQUEST:
                status = fileitem_status::INVALID_REQUEST;
                break;
            case filesystem_status::NO_AVAIL_INODES:
                status = fileitem_status::NO_AVAIL_INODES;
                break;
            default:
                status = fileitem_status::IO_ERROR;
        }
        return {.file = {}, .status = status};
    }
    auto readResult = Entries();
    if (readResult.status != fileitem_status::SUCCESS) {
        return {.file = {}, .status = readResult.status};
    }
    allocInode.inode->Init();
    allocInode.inode->dirty = true;
    std::shared_ptr<ext2fs_file> file = std::make_shared<ext2fs_file>(fs, allocInode.inode);

    auto seekTo = actualSize;
    ext2fs_inode_reader reader{inode};
    reader.seek_set(seekTo);

    ext2dirent *dirent;
    dirent = new (malloc(sizeof(*dirent) + filename.size())) ext2dirent();
    dirent->inode = allocInode.inode->inode;
    dirent->file_type = Ext2FileType_Regular;
    dirent->name_len = filename.size();
    dirent->rec_len = sizeof(*dirent) + filename.size();
    char *filenameArea = ((char *) dirent) + sizeof(*dirent);
    memcpy(filenameArea, filename.data(), filename.size());

    actualSize += dirent->rec_len;
    if (actualSize > inode->GetFileSize()) {
        auto blocksize = BlockSize();
        auto blocks = actualSize / blocksize;
        if ((actualSize % blocksize) != 0) {
            ++blocks;
        }
        inode->SetFileSize(blocks * blocksize);
    }

    auto result = reader.write(dirent, dirent->rec_len);
    dirent->~ext2dirent();
    free(dirent);
    if (result.status != filesystem_status::SUCCESS) {
        switch (result.status) {
            case filesystem_status::IO_ERROR:
                return {.file = {}, .status = fileitem_status::IO_ERROR};
            case filesystem_status::INTEGRITY_ERROR:
                return {.file = {}, .status = fileitem_status::INTEGRITY_ERROR};
            case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
                return {.file = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
            case filesystem_status::INVALID_REQUEST:
                return {.file = {}, .status = fileitem_status::INVALID_REQUEST};
            case filesystem_status::NO_AVAIL_INODES:
                return {.file = {}, .status = fileitem_status::NO_AVAIL_INODES};
            case filesystem_status::NO_AVAIL_BLOCKS:
                return {.file = {}, .status = fileitem_status::NO_AVAIL_BLOCKS};
        }
    }

    return {.file = file, .status = fileitem_status::SUCCESS};
}
