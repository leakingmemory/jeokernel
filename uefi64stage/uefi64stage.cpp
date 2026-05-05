//
// Created by sigsegv on 4/17/26.
//

#include <cstdint>

#include "stage1.h"
#include "uefistage/uefistage.h"

extern "C" {
    void stage_main(UefiStageContext *ctx) {
        uint8_t fxdata alignas(16) [512];
        uint64_t tmpval alignas(16);

        asm("mov $0x10,%%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%gs; mov %%ax, %%ss" ::: "%rax");

        {
            // FPU config
            {
                tmpval = 0x037F;
                asm("fninit; fldcw %0" :: "m"(tmpval));
            }
            // SSE FP enable:
            {
                uint64_t cr0;
                asm("mov %%cr0,%0":"=r"(cr0));
                cr0 &= 0xFFFFFFFFFFFFFFFBUL; // coprocessor emu off
                cr0 |= 2; // coprocessor monitor
                asm("mov %0,%%cr0"::"r"(cr0));
            }
            {
                uint64_t cr4;
                asm("mov %%cr4,%0":"=r"(cr4));
                cr4 |= 0x700; // FXSR XMMSEXCEPT
                asm("mov %0,%%cr4"::"r"(cr4));
            }
            {
                uint64_t addr{reinterpret_cast<uint64_t>(&fxdata[0])};
                asm("fxsave (%0)" :: "r"(addr));
                uint32_t mxcsr_mask = *((uint32_t *) (addr + 28));
                if (mxcsr_mask == 0) {
                    mxcsr_mask = 0xFFBF;
                }
                uint32_t mxcsr = 0x1F80 & mxcsr_mask;
                asm("ldmxcsr %0" :: "m"(mxcsr));
            }

            auto cr3 = ctx->vmem_root;
            asm("mov %0,%%cr3; " :: "r"(cr3));
            asm("mov %%cr3, %0; " : "=r"(cr3));
            //print(L"cr3=");
            //print_u64(cr3);
            uint64_t cr4;
            asm("mov %%cr4, %0; " : "=r"(cr4));
            cr4 |= 1 << 5;
            asm("mov %0, %%cr4; " :: "r"(cr4));
            asm("mov %%cr4, %0; " : "=r"(cr4));

            // enable long mode: 0x100 (1 << 8) and 0x800 (1 << 11)
            asm("mov $0x0C0000080, %%rcx; rdmsr; or $0x0900, %%rax; wrmsr; " ::: "%rax", "%rcx");

            uint64_t cr0;
            asm("mov %%cr0, %0" : "=r"(cr0));

            cr0 |= 1 << 16; // Write protected memory enabled
            cr0 |= 1 << 31; // Paging bit = 1
            asm("mov %0, %%cr0" :: "r"(cr0));

        }

        uint64_t uefiMemMap = ctx->efi_memory_map_page_aligned_plus_descriptor_size;
        uint64_t uefiMemMapNumRecords = ctx->efi_memory_map_size;
        auto uefiMemMapDescrSize = static_cast<uint32_t>(uefiMemMap & 0xFFF);
        uefiMemMapNumRecords = uefiMemMapNumRecords / uefiMemMapDescrSize;
        uefiMemMap = uefiMemMap / 0x1000;
        Stage1Data stage1Data = {
            .multibootAddr = static_cast<uint32_t>(0),
            .physpageMapAddr = static_cast<uint32_t>(ctx->physpage_map),
            .init_pml4t = static_cast<uint32_t>(ctx->pml4t_addr),
            .uefiMemoryMapPage = static_cast<uint32_t>(uefiMemMap),
            .uefiMemoryMapDescrSize = uefiMemMapDescrSize,
            .uefiMemoryMapNumDescr = static_cast<uint32_t>(uefiMemMapNumRecords),
            .gdtAddr = static_cast<uint32_t>(ctx->gdt_addr),
            .efi_horiz = static_cast<uint32_t>(ctx->efi_horiz),
            .efi_vert = static_cast<uint32_t>(ctx->efi_vert),
            .efi_pixel_format = static_cast<uint32_t>(ctx->efi_pixel_format),
            .kernel_phys = static_cast<uint32_t>(ctx->kernel_phys),
            .kernel_size = static_cast<uint32_t>(ctx->kernel_size),
            .efi_framebuffer = ctx->efi_framebuffer,
            .efi_framebuffer_size = ctx->efi_framebuffer_size,
            .efi_rsdp_ptr = ctx->efi_rsdp_ptr,
        };
        uint64_t stack_addr = reinterpret_cast<uint64_t>(&stage1Data);
        stack_addr &= 0xfffffffffffffff0ULL;
        uint64_t stage1DataPtr = (uint64_t) &stage1Data;
        uint64_t jmpAddr[2] = {ctx->entrypoint_addr, 8};
        uint64_t jmpAddrPtr = reinterpret_cast<uint64_t>(&(jmpAddr[0]));
        uint64_t entrypoint_addr = ctx->entrypoint_addr;

        //asm("mov $0x10,%%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%ss; " ::: "%ax");
        asm("mov %1, %%rbx; mov %2, %%rsi; jmpq *%0" :: "r"(entrypoint_addr), "r"(stage1DataPtr), "r"(stack_addr));

        while (1) {
            asm("hlt");
        }
    }
}
