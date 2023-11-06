//
// Created by sigsegv on 10/5/23.
//

#ifndef FSBITS_EXT2FS_DIRECTORY_H
#define FSBITS_EXT2FS_DIRECTORY_H

#include "ext2fs_file.h"
#include <vector>
#include <memory>

class ext2fs;

class ext2fs_directory : public directory, public ext2fs_file {
private:
    std::vector<std::shared_ptr<directory_entry>> entries;
    std::size_t lastDirentPos, actualSize;
    bool entriesRead;
public:
    ext2fs_directory(std::shared_ptr<ext2fs> fs) : directory(), ext2fs_file(fs), entries(), lastDirentPos(0), actualSize(0), entriesRead(false) {}
    std::string GetReferrerIdentifier() override;
    entries_result Entries() override;

    directory_resolve_result Create(std::string filename, uint16_t mode, uint8_t filetype);
    directory_resolve_result CreateFile(std::string filename, uint16_t mode) override;
    void InitializeDirectory(uint32_t parentInode, uint32_t blocknum);
    directory_resolve_result CreateDirectory(std::string filename, uint16_t mode) override;

private:
    ext2fs &Filesystem() {
        return *(this->fs);
    }
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};


#endif //FSBITS_EXT2FS_DIRECTORY_H
