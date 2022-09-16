//
// Created by sigsegv on 4/23/22.
//

#ifndef FSBITS_FILEITEM_H
#define FSBITS_FILEITEM_H

#include <cstdint>
#include <files/filepage.h>

class fileitem {
public:
    virtual ~fileitem() = default;
    virtual uint32_t Mode() = 0;
    virtual std::size_t Size() = 0;
    virtual uintptr_t InodeNum() = 0;
    virtual uint32_t BlockSize() = 0;
    virtual uintptr_t SysDevId() = 0;
    virtual std::shared_ptr<filepage> GetPage(std::size_t pagenum) = 0;
    virtual std::size_t Read(uint64_t offset, void *ptr, std::size_t length) = 0;
};

#endif //FSBITS_FILEITEM_H
