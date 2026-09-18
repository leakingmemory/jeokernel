//
// Created by sigsegv on 3/17/22.
//

#ifndef JEOKERNEL_STAGE1_H
#define JEOKERNEL_STAGE1_H

#include <stdint.h>
#if defined(__aarch64__)
#include <concurrency/raw_spinlock.h>
#endif

struct PhysMemFree {
    uint32_t page;
    uint32_t num;
};

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

    uint64_t ppmap;
    uint64_t ppmap_base_page;

    uint64_t vpalloc_root_vaddr;

    uint64_t mem_mapper_8pages;

    uint32_t cpu_count;

    static constexpr uint32_t phys_mem_free_max = 2;
    uint32_t phys_mem_free_count{0};

    uint32_t phys_mem_free_page[phys_mem_free_max];
    uint32_t phys_mem_free_num[phys_mem_free_max];

    raw_spinlock early_init_lock;
    raw_spinlock smp_synch_lock;
    uint32_t boot_stage_counter;

    constexpr void AddFreePhys(uint32_t page, uint32_t num) {
        if (phys_mem_free_count < phys_mem_free_max) {
            phys_mem_free_page[phys_mem_free_count] = page;
            phys_mem_free_num[phys_mem_free_count] = num;
            phys_mem_free_count++;
        }
    }
    constexpr PhysMemFree GetFreePhys() {
        if (phys_mem_free_count < 1) {
            return {};
        }
        auto page = phys_mem_free_page[phys_mem_free_count - 1];
        auto num = phys_mem_free_num[phys_mem_free_count - 1];
        --phys_mem_free_count;
        return {.page = page, .num = num};
    }
    constexpr bool HasFreePhys() const {
        return phys_mem_free_count > 0;
    }
#endif
};

#endif //JEOKERNEL_STAGE1_H
