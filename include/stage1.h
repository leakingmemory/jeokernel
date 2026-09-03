//
// Created by sigsegv on 3/17/22.
//

#ifndef JEOKERNEL_STAGE1_H
#define JEOKERNEL_STAGE1_H

#include <stdint.h>
#if defined(__aarch64__)
#include <concurrency/raw_spinlock.h>
#endif

struct Stage1Data {
#if defined(__x86_64__) || defined(__i386__)
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

    uint32_t kernel_phys;
    uint32_t kernel_size;

    uint64_t efi_framebuffer;

    uint64_t efi_framebuffer_size;

    uint64_t efi_rsdp_ptr;
#elif defined(__aarch64__)
    uint64_t uart;

    uint64_t dtb;

    uint64_t phys_mem_base;

    uint64_t phys_mem_size;

    uint64_t root_pt;

    uint64_t cpu_count;

    raw_spinlock early_init_lock;
#endif
};

#endif //JEOKERNEL_STAGE1_H
