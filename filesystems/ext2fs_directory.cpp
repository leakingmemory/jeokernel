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

constexpr uint16_t modeTypeFile =      00100000;
constexpr uint16_t modeTypeDirectory = 00040000;

static directory_entry *CreateDirectoryEntry(std::shared_ptr<filesystem> fs, ext2dirent *dirent, filesystem_status &error) {
    std::string name{dirent->Name()};
    auto *e2fs = (ext2fs *) &(*fs);
    if (dirent->file_type == Ext2FileType_Directory) {
        auto result = e2fs->GetDirectory(fs, dirent->inode);
        if (result.node) {
            return new directory_entry(name, result.node);
        } else {
            error = result.status;
            return nullptr;
        }
    } else if (dirent->file_type == Ext2FileType_Regular) {
        auto result = e2fs->GetFile(fs, dirent->inode);
        if (result.node) {
            return new directory_entry(name, result.node);
        } else {
            error = result.status;
            return nullptr;
        }
    } else if (dirent->file_type == Ext2FileType_Symlink) {
        auto result = e2fs->GetSymlink(fs, dirent->inode);
        if (result.node) {
            return new directory_entry(name, result.node);
        } else {
            error = result.status;
            return nullptr;
        }
    }
    error = filesystem_status::NOT_SUPPORTED_FS_FEATURE;
    return nullptr;
}

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
                lastDirentPos = inode->GetFileSize() - sizeRemaining;
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
                        filesystem_status error{};
                        auto *direntObj = CreateDirectoryEntry(fs, dirent, error);
                        if (direntObj != nullptr) {
                            entries.emplace_back(direntObj);
                        }  else {
                            return {.entries = {}, .status = Convert(error)};
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

directory_resolve_result ext2fs_directory::Create(std::string filename, uint16_t mode, uint8_t filetype) {
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
        Filesystem().ReleaseInode(allocInode.inode->inode);
        return {.file = {}, .status = readResult.status};
    }

    for (auto &entry : readResult.entries) {
        if (entry->Name() == filename) {
            Filesystem().ReleaseInode(allocInode.inode->inode);
            return {.file = {}, .status = fileitem_status::EXISTS};
        }
    }

    allocInode.inode->Init();
    allocInode.inode->linkCount = 1;
    allocInode.inode->mode = mode;
    allocInode.inode->uid = 0;
    allocInode.inode->atime = 0;
    allocInode.inode->ctime = 0;
    allocInode.inode->mtime = 0;
    allocInode.inode->dtime = 0;
    allocInode.inode->gid = 0;
    allocInode.inode->flags = 0;
    allocInode.inode->generation = 0;
    allocInode.inode->file_acl = 0;
    allocInode.inode->dir_acl = 0;
    allocInode.inode->fragment_address = 0;
    allocInode.inode->fragment_number = 0;
    allocInode.inode->fragment_size = 0;
    allocInode.inode->dirty = true;

    auto seekTo = lastDirentPos;
    ext2fs_inode_reader reader{inode};
    reader.seek_set(seekTo);

    ext2dirent *lastDirent, *dirent;
    auto sizeofLast = actualSize - lastDirentPos;

    {
        lastDirent = sizeofLast >= sizeof(*dirent) ? new(malloc(sizeofLast)) ext2dirent() : nullptr;
        if (lastDirent != nullptr) {
            auto result = reader.read(lastDirent, sizeofLast);
            if (result.status != filesystem_status::SUCCESS) {
                Filesystem().ReleaseInode(allocInode.inode->inode);
                lastDirent->~ext2dirent();
                free(lastDirent);
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
                    default:
                        return {.file = {}, .status = fileitem_status::IO_ERROR};
                }
            }
            if (result.size != sizeofLast) {
                Filesystem().ReleaseInode(allocInode.inode->inode);
                lastDirent->~ext2dirent();
                free(lastDirent);
                return {.file = {}, .status = fileitem_status::INTEGRITY_ERROR};
            }
            auto minimumLength = lastDirent->MinimumLength();
            auto padding = 4 - (minimumLength & 3);
            auto minimumPaddedLength = minimumLength + padding;
            if (minimumPaddedLength < lastDirent->rec_len) {
                lastDirent->rec_len = minimumPaddedLength;
                bzero(lastDirent->PaddingPtr(), padding);
                auto status = reader.seek_set(seekTo);
                if (status == filesystem_status::SUCCESS) {
                    auto result = reader.write(lastDirent, lastDirent->rec_len);
                    if (result.status == filesystem_status::SUCCESS) {
                        if (result.size != lastDirent->rec_len) {
                            status = filesystem_status::IO_ERROR;
                        }
                    } else {
                        status = result.status;
                    }
                }
                if (status != filesystem_status::SUCCESS) {
                    Filesystem().ReleaseInode(allocInode.inode->inode);
                    lastDirent->~ext2dirent();
                    free(lastDirent);
                    switch (status) {
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
                        default:
                            return {.file = {}, .status = fileitem_status::IO_ERROR};
                    }
                }
                seekTo += lastDirent->rec_len;
            }
            lastDirent->~ext2dirent();
            free(lastDirent);
        }
    }
    auto direntSize = sizeof(*dirent) + filename.size();
    auto padding = 4 - (direntSize & 3);
    direntSize += padding;
    dirent = new (malloc(direntSize)) ext2dirent();
    dirent->inode = allocInode.inode->inode;
    dirent->file_type = filetype;
    dirent->name_len = filename.size();
    dirent->rec_len = direntSize;
    char *filenameArea = ((char *) dirent) + sizeof(*dirent);
    memcpy(filenameArea, filename.data(), filename.size());
    bzero(dirent->PaddingPtr(), padding);

    lastDirentPos = seekTo;
    actualSize = seekTo + dirent->rec_len;
    if (actualSize > inode->GetFileSize()) {
        auto blocksize = BlockSize();
        auto blocks = actualSize / blocksize;
        if ((actualSize % blocksize) != 0) {
            ++blocks;
        }
        inode->SetFileSize(blocks * blocksize);
    }
    actualSize = inode->GetFileSize();
    auto rec_len = dirent->rec_len;
    dirent->rec_len = actualSize - seekTo;

    auto result = reader.write(dirent, rec_len);

    std::shared_ptr<fileitem> file{};
    if (result.status == filesystem_status::SUCCESS) {
        filesystem_status error{};
        auto *direntObj = CreateDirectoryEntry(fs, dirent, error);
        if (direntObj != nullptr) {
            auto &item = entries.emplace_back(direntObj);
            file = item->Item();
        }  else {
            if (error == filesystem_status::SUCCESS) {
                error = filesystem_status::INTEGRITY_ERROR;
            }
            result.status = error;
        }
    }

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
            default:
                return {.file = {}, .status = fileitem_status::IO_ERROR};
        }
    }

    return {.file = file, .status = fileitem_status::SUCCESS};
}

directory_resolve_result ext2fs_directory::CreateFile(std::string filename, uint16_t mode) {
    return Create(filename, mode | modeTypeFile, Ext2FileType_Regular);
}

void ext2fs_directory::InitializeDirectory(uint32_t parentInode, uint32_t blocknum) {
    auto blocksize = BlockSize();
    auto *page = new filepage();
    page->Zero();
    constexpr auto sz2e = sizeof(ext2dirent);
    constexpr auto pad2e = 4 - ((sz2e + 2) & 3);
    constexpr auto totsize = sz2e + pad2e + 2;
    std::shared_ptr<ext2dirent> d2{new (malloc(totsize)) ext2dirent()};
    static_assert(sizeof(*d2) == sz2e);
    char *nameptr = ((char *) &(*d2)) + sz2e;
    nameptr[0] = '.';
    nameptr[1] = '.';
    bzero(nameptr + 2, pad2e);
    d2->inode = inode->inode;
    d2->rec_len = totsize;
    d2->name_len = 1;
    d2->file_type = Ext2FileType_Directory;
    memcpy(page->Pointer()->Pointer(), &(*d2), d2->MinimumLength());
    uint16_t offset = d2->rec_len;
    d2->inode = parentInode;
    d2->rec_len = blocksize - offset;
    d2->name_len = 2;
    memcpy(((uint8_t *) page->Pointer()->Pointer()) + offset, &(*d2), d2->MinimumLength());
    page->SetDirty(blocksize);
    inode->blockRefs.push_back(blocknum);
    inode->blockCache.emplace_back(page);
    inode->filesize = blocksize;
    inode->linkCount = 2;
    inode->dirty = true;
}

directory_resolve_result ext2fs_directory::CreateDirectory(std::string filename, uint16_t mode) {
    auto emptyDirectoryBlock = Filesystem().AllocateBlocks(1);
    if (emptyDirectoryBlock.status != filesystem_status::SUCCESS) {
        return {.file = {}, .status = Convert(emptyDirectoryBlock.status)};
    }
    auto inodeCreateResult = Create(filename, mode | modeTypeDirectory, Ext2FileType_Directory);
    auto *dir = dynamic_cast<ext2fs_directory *>(&(*(inodeCreateResult.file)));
    if (inodeCreateResult.status == fileitem_status::SUCCESS && dir == nullptr) {
        inodeCreateResult.status = fileitem_status::INTEGRITY_ERROR;
    }
    if (inodeCreateResult.status != fileitem_status::SUCCESS) {
        Filesystem().ReleaseBlock(emptyDirectoryBlock.block);
        return inodeCreateResult;
    }
    auto inodeNum = inode->inode;
    dir->InitializeDirectory(inodeNum, emptyDirectoryBlock.block);
    inode->linkCount++;
    inode->dirty = true;
    Filesystem().IncrementDirCount(inodeNum);
    return inodeCreateResult;
}
