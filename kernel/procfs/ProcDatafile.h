//
// Created by sigsegv on 2/23/23.
//

#ifndef JEOKERNEL_PROCDATAFILE_H
#define JEOKERNEL_PROCDATAFILE_H

#include <files/fileitem.h>

class ProcDatafile : public fileitem {
private:
    uint32_t mode;
public:
    ProcDatafile() : mode(00400) {}
    uint32_t Mode() override;
    void SetMode(uint32_t mode);
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    std::size_t Size() override = 0;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override = 0;
};


#endif //JEOKERNEL_PROCDATAFILE_H
