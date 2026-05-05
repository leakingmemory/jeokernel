//
// Created by sigsegv on 4/19/26.
//

#ifndef JEOKERNEL_UEFISTAGE_H
#define JEOKERNEL_UEFISTAGE_H

struct UefiStageContext {
    uint64_t vmem_root; // cr3
    uint64_t physpage_map;
    uint64_t entrypoint_addr;
    uint64_t pml4t_addr;
    uint64_t efi_memory_map_page_aligned_plus_descriptor_size;
    uint64_t efi_memory_map_size;
    uint64_t gdt_addr;
    uint64_t efi_horiz;
    uint64_t efi_vert;
    uint64_t efi_pixel_format;
    uint64_t kernel_phys;
    uint64_t kernel_size;
    uint64_t efi_framebuffer;
    uint64_t efi_framebuffer_size;
    uint64_t efi_rsdp_ptr;
};

#endif //JEOKERNEL_UEFISTAGE_H
