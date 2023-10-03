//
// Created by sigsegv on 4/23/22.
//

#ifndef FSBITS_FILEITEM_H
#define FSBITS_FILEITEM_H

#include <cstdint>
#include <files/filepage.h>
#include <string>

enum class fileitem_status {
    SUCCESS,
    IO_ERROR,
    INTEGRITY_ERROR,
    NOT_SUPPORTED_FS_FEATURE,
    INVALID_REQUEST,
    TOO_MANY_LINKS,
    NO_AVAIL_INODES,
    NO_AVAIL_BLOCKS
};

std::string text(fileitem_status status);

struct file_getpage_result {
    std::shared_ptr<filepage> page;
    fileitem_status status;
};

struct file_read_result {
    std::size_t size;
    fileitem_status status;
};

class fileitem {
 public:
    virtual ~fileitem() = default;
    virtual uint32_t Mode() = 0;
    virtual std::size_t Size() = 0;
    virtual uintptr_t InodeNum() = 0;
    virtual uint32_t BlockSize() = 0;
    virtual uintptr_t SysDevId() = 0;
    virtual file_getpage_result GetPage(std::size_t pagenum) = 0;
    virtual file_read_result Read(uint64_t offset, void *ptr, std::size_t length) = 0;
};

#endif //FSBITS_FILEITEM_H
