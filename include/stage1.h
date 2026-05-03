//
// Created by sigsegv on 3/17/22.
//

#ifndef JEOKERNEL_STAGE1_H
#define JEOKERNEL_STAGE1_H

#include <cstdint>

struct Stage1Data {
    uint32_t multibootAddr;
    uint32_t physpageMapAddr;

    uint32_t init_pml4t;
    uint32_t uefiMemoryMapPage;

    uint32_t uefiMemoryMapDescrSize;
    uint32_t uefiMemoryMapNumDescr;

    uint32_t gdtAddr;
    uint32_t efi_horiz;

    uint32_t efi_vert;
    uint32_t efi_pixel_format;

    uint64_t efi_framebuffer;

    uint64_t efi_framebuffer_size;
};

#endif //JEOKERNEL_STAGE1_H
