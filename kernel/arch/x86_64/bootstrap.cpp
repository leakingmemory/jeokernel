//
// Created by sigsegv on 3/16/22.
//

#include "bootstrap.h"
#include <stage1.h>
#include <multiboot.h>
#include <stack.h>
#include <pagealloc.h>
#include <pagetable.h>
#include <physpagemap.h>
#include <core/malloc.h>

// Boot state captured from Stage1Data; declared in bootstrap.h and read by the
// rest of stage 1 init (m64-stage1.cpp).
const MultibootInfoHeader *multiboot_info = nullptr;
uint32_t uefiMemoryMapPage;
uint32_t uefiMemoryMapDescrSize;
uint32_t uefiMemoryMapNumDescr;
uint32_t loaderGdtAddr;
uint32_t efi_horiz;
uint32_t efi_vert;
uint32_t efi_pixel_format;
uint32_t kernel_phys;
uint32_t kernel_size;
uint64_t efi_framebuffer;
uint64_t efi_framebuffer_size;
uint64_t efi_rsdp_ptr;

static normal_stack *stage1_stack = nullptr;

extern "C" {

    void start_m64(Stage1Data *stage1Data, uint64_t stackptr) {
        const MultibootInfoHeader *multibootInfoHeaderPtr = (const MultibootInfoHeader *) (void *) (uint64_t) stage1Data->multibootAddr;
        uint64_t physmapaddr = stage1Data->physpageMapAddr;

        multiboot_info = multibootInfoHeaderPtr;
        uefiMemoryMapPage = stage1Data->uefiMemoryMapPage;
        uefiMemoryMapDescrSize = stage1Data->uefiMemoryMapDescrSize;
        uefiMemoryMapNumDescr = stage1Data->uefiMemoryMapNumDescr;
        loaderGdtAddr = stage1Data->gdtAddr;
        efi_horiz = stage1Data->efi_horiz;
        efi_vert = stage1Data->efi_vert;
        efi_pixel_format = stage1Data->efi_pixel_format;
        kernel_phys = stage1Data->kernel_phys;
        kernel_size = stage1Data->kernel_size;
        efi_framebuffer = stage1Data->efi_framebuffer;
        efi_framebuffer_size = stage1Data->efi_framebuffer_size;
        efi_rsdp_ptr = stage1Data->efi_rsdp_ptr;
        /*
         * Let's try to alloc a stack
         */
        set_init_pml4t(stage1Data->init_pml4t);
        init_simple_physpagemap(physmapaddr, 0);
        initialize_pagetable_control();
        setup_simplest_malloc_impl();
        extend_to_advanced_physpagemap();

        auto *ap_boot_stack = new normal_stack;
        *((uint64_t *) 0x8150) = ap_boot_stack->get_addr();

        stage1_stack = new normal_stack;
        uint64_t stack = stage1_stack->get_addr();
        asm("mov %0, %%rax; mov %%rax,%%rsp; jmp secondboot; hlt;" :: "r"(stack) : "%rax");
    }

}
