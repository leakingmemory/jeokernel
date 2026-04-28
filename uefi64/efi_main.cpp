//
// Created by sigsegv on 4/8/26.
//

#include <uefi.h>
#include <new>

#include <pagetable.h>
#include <pagetable_impl.h>
#include <physpagemap.h>
#include <elf.h>
#include <elf_impl.h>

#include "loaderconfig.h"
#include "stage1.h"
#include "uefistage/uefistage.h"

typedef void* EFI_HANDLE;

static efi_system_table *efi_system_table_ptr;

uintptr_t get_pagetable_virt_offset() {
    return 0;
}

static int print(const wchar_t *str) {
    return efi_system_table_ptr->con_out->output_string(efi_system_table_ptr->con_out, str);
}

#define CONFIG_BELOW_ADDRESS 0x400000
#define CONFIG_BELOW_PAGE (CONFIG_BELOW_ADDRESS / 0x1000)

static void *allocate_config_pages(int n) {
    void *ptr{reinterpret_cast<void *>(static_cast<uint64_t>(CONFIG_BELOW_ADDRESS) < (static_cast<uint64_t>(PhyspageMap::max_pageaddr()) * 0x1000ULL) ? static_cast<uint64_t>(CONFIG_BELOW_ADDRESS) : (static_cast<uint64_t>(PhyspageMap::max_pageaddr()) * 0x1000ULL))};

    auto res = efi_system_table_ptr->boot_services->allocate_pages(AllocateMaxAddress, 0x80000000, n, &ptr);
    if (res != 0) {
        return nullptr;
    }
    return ptr;
}

static void *allocate_pages(int n) {
    void *ptr{reinterpret_cast<void *>(static_cast<uint64_t>(PhyspageMap::max_pageaddr()) * 0x1000ULL)};
    auto res = efi_system_table_ptr->boot_services->allocate_pages(AllocateMaxAddress, 0x80000000, n, &ptr);
    if (res != 0) {
        return nullptr;
    }
    return ptr;
}

template <typename CharType, typename IntType, const int Num> void num_as_hex(CharType str[Num], IntType n) {
    for (int i = 0; i < Num; i++) {
        str[Num - i - 1] = "0123456789ABCDEF"[n & 0xf];
        n >>= 4;
    }
}

static int print_u32(uint32_t num) {
    wchar_t str[11];
    str[0] = '0';
    str[1] = 'x';
    num_as_hex<wchar_t,uint32_t,8>(str + 2, num);
    str[10] = '\0';
    return print(str);
}

static int print_i32(int32_t num) {
    uint32_t n{static_cast<uint32_t>(num)};
    wchar_t str[11];
    str[0] = '0';
    str[1] = 'x';
    num_as_hex<wchar_t,uint32_t,8>(str + 2, n);
    str[10] = '\0';
    return print(str);
}

static int print_u64(uint64_t num) {
    wchar_t str[19];
    str[0] = '0';
    str[1] = 'x';
    num_as_hex<wchar_t,uint64_t,16>(str + 2, num);
    str[18] = '\0';
    return print(str);
}

static int print_ptr(const void *ptr) {
    return print_u64(reinterpret_cast<uint64_t>(ptr));
}

extern "C" {

    extern const void *wrapped_binary;
    extern const void *wrapped_uefistage_start;
    extern const void *wrapped_uefistage_end;

    void EFIAPI efi_main(EFI_HANDLE ImageHandle, void *ptr_SystemTable) {
        efi_system_table_ptr = reinterpret_cast<efi_system_table *>(ptr_SystemTable);
        uint8_t *stageloader_src = reinterpret_cast<uint8_t *>(&wrapped_uefistage_start);
        uint8_t *kernel_src = reinterpret_cast<uint8_t *>(&wrapped_binary);
        print(L"UEFI shimloader detected entrypoint: ");
        print_ptr(reinterpret_cast<void *>(&efi_main));
        print(L"\r\nStage loader embedded binary: ");
        print_ptr(wrapped_uefistage_start);
        print(L"\r\nKernel embedded binary: ");
        print_ptr(kernel_src);
        print(L"\r\nStage loader size: ");
        auto stageloader_size = reinterpret_cast<uintptr_t>(&wrapped_uefistage_end) - reinterpret_cast<uintptr_t>(stageloader_src);
        print_u32(static_cast<uint32_t>(stageloader_size));
        auto stageloader_pages = static_cast<uint32_t>(stageloader_size / 0x1000);
        if ((stageloader_size % 0x1000) != 0) {
            ++stageloader_pages;
        }
        print(L" (pages: ");
        print_u32(stageloader_pages);
        void *phys_stageloader = allocate_config_pages(stageloader_pages);
        print(L")\r\n");
        if (phys_stageloader == nullptr){
            print(L"FATAL ERROR: Failed to allocate kernel pages");
            asm("ud2");
        }
        memcpy(phys_stageloader, stageloader_src, stageloader_size);
        print(L"Kernel size: ");
        auto kernel_size = reinterpret_cast<uintptr_t>(stageloader_src) - reinterpret_cast<uintptr_t>(kernel_src);
        print_u32(static_cast<uint32_t>(kernel_size));
        auto kernel_pages = static_cast<uint32_t>(kernel_size / 0x1000);
        if ((kernel_size % 0x1000) != 0) {
            ++kernel_pages;
        }
        print(L" (pages: ");
        print_u32(kernel_pages);
        void *phys_kernel = allocate_pages(kernel_pages);
        print(L")\r\n");
        if (phys_kernel == nullptr){
            print(L"FATAL ERROR: Failed to allocate kernel pages");
            asm("ud2");
        }
        memcpy(phys_kernel, kernel_src, kernel_size);
        print(L"Kernel phys addr: ");
        print_ptr(phys_kernel);
        print(L"\r\n");
        ELF stageloader{(void *) stageloader_src, (void *) &wrapped_uefistage_end};
        if (!stageloader.is_valid()) {
            print(L"FATAL ERROR: stageloader binary is not valid");
            asm("ud2");
        }
        ELF kernel{(void *) kernel_src, (void *) stageloader_src};
        if (!kernel.is_valid()) {
            print(L"FATAL ERROR: Kernel binary is not valid");
            asm("ud2");
        }
        const auto &stageloader_header = stageloader.get_elf64_header();
        uint64_t stageloader_entrypoint_addr = stageloader_header.e_entry;
        print(L"Stageloader entrypoint: ");
        print_u64(stageloader_entrypoint_addr);
        print(L"\r\n");
        uintptr_t stageloader_vmem_start;
        {
            bool loaded_first_offset{false};
            uintptr_t first_offset;
            for (uint16_t i = 0; i < stageloader_header.e_phnum; i++) {
                const auto &ph = stageloader_header.get_program_entry(i);
                if (ph.p_type != PHT_LOAD || ph.p_memsz <= 0) {
                    continue;
                }
                if (!loaded_first_offset || ph.p_offset < first_offset) {
                    first_offset = ph.p_offset;
                    stageloader_vmem_start = ph.p_vaddr;
                    loaded_first_offset = true;
                }
            }
            if (!loaded_first_offset) {
                print(L"FATAL ERROR: No loadable sections found in stageloader binary");
                asm("ud2");
            }
            if (stageloader_vmem_start < first_offset) {
                print(L"FATAL ERROR: Stageloader binary is not properly relocated");
                asm("ud2");
            }
            stageloader_vmem_start -= first_offset;
            if (stageloader_vmem_start % 0x1000 != 0) {
                print(L"FATAL ERROR: Stageloader binary is not properly page aligned");
                asm("ud2");
            }
        }
        print(L"Stageloader virtual start addr: ");
        print_u64(stageloader_vmem_start);
        const auto &elf64_header = kernel.get_elf64_header();
        uint64_t entrypoint_addr = elf64_header.e_entry;
        print(L"Kernel entrypoint: ");
        print_u64(entrypoint_addr);
        print(L"\r\n");
        uintptr_t kernel_vmem_start;
        {
            bool loaded_first_offset{false};
            uintptr_t first_offset;
            for (uint16_t i = 0; i < elf64_header.e_phnum; i++) {
                const auto &ph = elf64_header.get_program_entry(i);
                if (ph.p_type != PHT_LOAD || ph.p_memsz <= 0) {
                    continue;
                }
                if (!loaded_first_offset || ph.p_offset < first_offset) {
                    first_offset = ph.p_offset;
                    kernel_vmem_start = ph.p_vaddr;
                    loaded_first_offset = true;
                }
            }
            if (!loaded_first_offset) {
                print(L"FATAL ERROR: No loadable sections found in kernel binary");
                asm("ud2");
            }
            if (kernel_vmem_start < first_offset) {
                print(L"FATAL ERROR: Kernel binary is not properly relocated");
                asm("ud2");
            }
            kernel_vmem_start -= first_offset;
            if (kernel_vmem_start % 0x1000 != 0) {
                print(L"FATAL ERROR: Kernel binary is not properly page aligned");
                asm("ud2");
            }
        }
        print(L"Kernel virtual start addr: ");
        print_u64(kernel_vmem_start);
        if (kernel_vmem_start < CONFIG_BELOW_ADDRESS) {
            print(L"FATAL ERROR: Kernel virtual address must start above CONFIG_BELOW_ADDRESS");
            asm("ud2");
        }
        print(L"\r\nPhyspages for memory management structs: ");
        constexpr const int conf_pages = 0x22;
        auto *physpageStructsAddr = (uint8_t *) allocate_config_pages(conf_pages);
        print_ptr(physpageStructsAddr);
        print(L"\r\n");
        auto *physpageMap = new (physpageStructsAddr) PhyspageMap();
        phys_t gdtAddr = reinterpret_cast<phys_t>(physpageStructsAddr) + 0x21000;

        {
            uint32_t k_base_page{static_cast<uint32_t>(reinterpret_cast<uint64_t>(phys_kernel) / 0x1000)};
            for (uint32_t i = 0; i < kernel_pages; i++) {
                uint32_t claim_pg{k_base_page + i};
                physpageMap->claim(claim_pg);
                //print(L"Claimed page: ");
                //print_u32(claim_pg);
                //print(L"\r\n");
            }
        }
        for (int i = 0; i < conf_pages; i++) {
            physpageMap->claim((reinterpret_cast<uint64_t>(physpageStructsAddr) / 0x1000) + i);
        }

        uint64_t (&pml4t_raw)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1000));
        pageentr (&pml4t)[512] = (pageentr (&)[512]) pml4t_raw;
        uint64_t (&pdpt_raw)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x2000));
        pageentr (&pdpt)[512] = (pageentr (&)[512]) pdpt_raw;
        uint64_t (&pdt0_raw)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x3000));
        pageentr (&pdt0)[512] = (pageentr (&)[512]) pdt0_raw;
        uint64_t (&pdt_raw)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x4000));
        pageentr (&pdt)[512] = (pageentr (&)[512]) pdt_raw;
        uint64_t (&pt_raw)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0xa000));
        pageentr (&pt)[512] = (pageentr (&)[512]) pt_raw;
        uint64_t (&pt_raw2)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0xb000));
        pageentr (&pt2)[512] = (pageentr (&)[512]) pt_raw2;
        uint64_t (&pt_raw3)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0xc000));
        pageentr (&pt3)[512] = (pageentr (&)[512]) pt_raw3;
        uint64_t (&pt_raw4)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0xd000));
        pageentr (&pt4)[512] = (pageentr (&)[512]) pt_raw4;
        uint64_t (&pt_raw5)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0xe000));
        pageentr (&pt5)[512] = (pageentr (&)[512]) pt_raw5;
        uint64_t (&pt_raw6)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0xf000));
        pageentr (&pt6)[512] = (pageentr (&)[512]) pt_raw6;
        uint64_t (&pt_raw7)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x10000));
        pageentr (&pt7)[512] = (pageentr (&)[512]) pt_raw7;
        uint64_t (&pt_raw8)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x11000));
        pageentr (&pt8)[512] = (pageentr (&)[512]) pt_raw8;
        uint64_t (&pt_raw9)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x12000));
        pageentr (&pt9)[512] = (pageentr (&)[512]) pt_raw9;
        uint64_t (&pt_raw10)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x13000));
        pageentr (&pt10)[512] = (pageentr (&)[512]) pt_raw10;
        uint64_t (&pt_raw11)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x14000));
        pageentr (&pt11)[512] = (pageentr (&)[512]) pt_raw11;
        uint64_t (&pt_raw12)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x15000));
        pageentr (&pt12)[512] = (pageentr (&)[512]) pt_raw12;
        uint64_t (&pt_raw13)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x16000));
        pageentr (&pt13)[512] = (pageentr (&)[512]) pt_raw13;
        uint64_t (&pt_raw14)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x17000));
        pageentr (&pt14)[512] = (pageentr (&)[512]) pt_raw14;
        uint64_t (&pt_raw15)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x18000));
        pageentr (&pt15)[512] = (pageentr (&)[512]) pt_raw15;
        uint64_t (&pt_raw16)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x19000));
        pageentr (&pt16)[512] = (pageentr (&)[512]) pt_raw16;
        uint64_t (&pt_raw17)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1a000));
        pageentr (&pt17)[512] = (pageentr (&)[512]) pt_raw17;
        uint64_t (&pt_raw18)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1b000));
        pageentr (&pt18)[512] = (pageentr (&)[512]) pt_raw18;
        uint64_t (&pt_raw19)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1c000));
        pageentr (&pt19)[512] = (pageentr (&)[512]) pt_raw19;
        uint64_t (&pt_raw20)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1d000));
        pageentr (&pt20)[512] = (pageentr (&)[512]) pt_raw20;
        uint64_t (&pt_raw21)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1e000));
        pageentr (&pt21)[512] = (pageentr (&)[512]) pt_raw21;
        uint64_t (&pt_raw22)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x1f000));
        pageentr (&pt22)[512] = (pageentr (&)[512]) pt_raw22;
        uint64_t (&pt_raw23)[512] = *((uint64_t (*)[512]) (physpageStructsAddr + 0x20000));
        pageentr (&pt23)[512] = (pageentr (&)[512]) pt_raw23;

        for (uint16_t i = 0; i < 512; i++) {
            pml4t_raw[i] = 0;
            pdpt_raw[i] = 0;
            pdt0_raw[i] = 0;
            pdt_raw[i] = 0;
            pt_raw[i] = 0;
            pt_raw2[i] = 0;
            pt_raw3[i] = 0;
            pt_raw4[i] = 0;
            pt_raw5[i] = 0;
            pt_raw6[i] = 0;
            pt_raw7[i] = 0;
            pt_raw8[i] = 0;
            pt_raw9[i] = 0;
            pt_raw10[i] = 0;
            pt_raw11[i] = 0;
            pt_raw12[i] = 0;
            pt_raw13[i] = 0;
            pt_raw14[i] = 0;
            pt_raw15[i] = 0;
            pt_raw16[i] = 0;
            pt_raw17[i] = 0;
            pt_raw18[i] = 0;
            pt_raw19[i] = 0;
            pt_raw20[i] = 0;
            pt_raw21[i] = 0;
            pt_raw22[i] = 0;
            pt_raw23[i] = 0;
        }

        /* Root table: First point to pdpt */
        pml4t[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x2000) / 4096;
        pml4t[0].writeable() = 1;
        pml4t[0].present() = 1;
        pml4t[0].os_virt_avail() = 1;
        /* First tree: first point to pdt0/0x0 */
        pdpt[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x3000) / 4096;
        pdpt[0].writeable() = 1;
        pdpt[0].present() = 1;
        pdpt[0].os_virt_avail() = 1;
        /* skip 1/0x40000000 and 2/0x80000000 */
        /* 3 points to pdt/0xc0000000 */
        pdpt[3].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x4000) / 4096;
        pdpt[3].writeable() = 1;
        pdpt[3].present() = 1;
        pdpt[3].os_virt_avail() = 1;
        /* Next two is the pointers to the first 4MB virt addr space (pt/0x0, pt2/0x200000) */
        pdt0[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xa000) / 4096;
        pdt0[0].writeable() = 1;
        pdt0[0].present() = 1;
        pdt0[0].os_virt_avail() = 1;
        pdt0[1].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xb000) / 4096;
        pdt0[1].writeable() = 1;
        pdt0[1].present() = 1;
        pdt0[1].os_virt_avail() = 1;
        /* stack area for bootloader - addr space 10MB-12MB (pt3) */
        pdt0[5].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xc000) / 4096;
        pdt0[5].writeable() = 1;
        pdt0[5].present() = 1;
        pdt0[5].os_virt_avail() = 1;
        /* ** */
        /* addr 0xc0000000 */
        pdt[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xd000) / 4096;
        pdt[0].writeable() = 1;
        pdt[0].present() = 1;
        pdt[0].os_virt_avail() = 0;
        /* addr 0xc0200000 */
        pdt[1].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xe000) / 4096;
        pdt[1].writeable() = 1;
        pdt[1].present() = 1;
        pdt[1].os_virt_avail() = 0;
        /* addr 0xc0400000 */
        pdt[2].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xf000) / 4096;
        pdt[2].writeable() = 1;
        pdt[2].present() = 1;
        pdt[2].os_virt_avail() = 1;
        /* addr 0xc0600000 */
        pdt[3].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x10000) / 4096;
        pdt[3].writeable() = 1;
        pdt[3].present() = 1;
        pdt[3].os_virt_avail() = 1;
        /* addr 0xc0800000 */
        pdt[4].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x11000) / 4096;
        pdt[4].writeable() = 1;
        pdt[4].present() = 1;
        pdt[4].os_virt_avail() = 1;
        /* addr 0xc0a00000 */
        pdt[5].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x12000) / 4096;
        pdt[5].writeable() = 1;
        pdt[5].present() = 1;
        pdt[5].os_virt_avail() = 1;
        /* addr 0xc0c00000 */
        pdt[6].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x13000) / 4096;
        pdt[6].writeable() = 1;
        pdt[6].present() = 1;
        pdt[6].os_virt_avail() = 1;
        /* addr 0xc0e00000 */
        pdt[7].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x14000) / 4096;
        pdt[7].writeable() = 1;
        pdt[7].present() = 1;
        pdt[7].os_virt_avail() = 1;
        /* addr 0xc1000000 */
        pdt[8].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x15000) / 4096;
        pdt[8].writeable() = 1;
        pdt[8].present() = 1;
        pdt[8].os_virt_avail() = 1;
        /* addr 0xc1200000 */
        pdt[9].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x16000) / 4096;
        pdt[9].writeable() = 1;
        pdt[9].present() = 1;
        pdt[9].os_virt_avail() = 1;
        /* addr 0xc1400000 */
        pdt[10].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x17000) / 4096;
        pdt[10].writeable() = 1;
        pdt[10].present() = 1;
        pdt[10].os_virt_avail() = 1;
        /* addr 0xc1600000 */
        pdt[11].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x18000) / 4096;
        pdt[11].writeable() = 1;
        pdt[11].present() = 1;
        pdt[11].os_virt_avail() = 1;
        /* addr 0xc1800000 */
        pdt[12].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x19000) / 4096;
        pdt[12].writeable() = 1;
        pdt[12].present() = 1;
        pdt[12].os_virt_avail() = 1;
        /* addr 0xc1a00000 */
        pdt[13].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1a000) / 4096;
        pdt[13].writeable() = 1;
        pdt[13].present() = 1;
        pdt[13].os_virt_avail() = 1;
        /* addr 0xc1c00000 */
        pdt[14].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1b000) / 4096;
        pdt[14].writeable() = 1;
        pdt[14].present() = 1;
        pdt[14].os_virt_avail() = 1;
        /* addr 0xc1e00000 */
        pdt[15].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1c000) / 4096;
        pdt[15].writeable() = 1;
        pdt[15].present() = 1;
        pdt[15].os_virt_avail() = 1;
        /* addr 0xc2000000 */
        pdt[16].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1d000) / 4096;
        pdt[16].writeable() = 1;
        pdt[16].present() = 1;
        pdt[16].os_virt_avail() = 1;
        /* addr 0xc2200000 */
        pdt[17].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1e000) / 4096;
        pdt[17].writeable() = 1;
        pdt[17].present() = 1;
        pdt[17].os_virt_avail() = 1;
        /* addr 0xc2400000 */
        pdt[18].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1f000) / 4096;
        pdt[18].writeable() = 1;
        pdt[18].present() = 1;
        pdt[18].os_virt_avail() = 1;
        /* addr 0xc2600000 */
        pdt[19].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x20000) / 4096;
        pdt[19].writeable() = 1;
        pdt[19].present() = 1;
        pdt[19].os_virt_avail() = 1;
        /*
         * Make first 2MiB adressable,writable,execable
         */
        for (uint16_t i = 0; i < 512; i++) {
            if (i > 0) {
                pt[i].page_ppn() = i;
                pt[i].writeable() = 1;
                pt[i].present() = 1;
            }
            {
                pt2[i].page_ppn() = i + 512;
                pt2[i].writeable() = 1;
                pt2[i].present() = 1;
            }
            pt2[i].os_virt_avail() = 0;
            pt3[i].os_virt_avail() = 1;
            pt4[i].os_virt_avail() = 0;
            pt5[i].os_virt_avail() = 1;
            pt6[i].os_virt_avail() = 1;
            pt7[i].os_virt_avail() = 1;
            pt8[i].os_virt_avail() = 1;
            pt9[i].os_virt_avail() = 1;
            pt10[i].os_virt_avail() = 1;
            pt11[i].os_virt_avail() = 1;
            pt12[i].os_virt_avail() = 1;
            pt13[i].os_virt_avail() = 1;
            pt14[i].os_virt_avail() = 1;
            pt15[i].os_virt_avail() = 1;
            pt16[i].os_virt_avail() = 1;
            pt17[i].os_virt_avail() = 1;
            pt18[i].os_virt_avail() = 1;
            pt19[i].os_virt_avail() = 1;
            pt20[i].os_virt_avail() = 1;
            pt21[i].os_virt_avail() = 1;
            pt22[i].os_virt_avail() = 1;
            pt23[i].os_virt_avail() = 1;
        }
        /*
         * Make second half, and next five, allocateable
         */
        for (uint16_t i = 0; i < 512; i++) {
            if (i >= 256) {
                pt[i].os_virt_avail() = 1;
            } else {
                physpageMap->claim(i);
            }
            pt2[i].os_virt_avail() = 1;
            pt3[i].os_virt_avail() = 1;
            pt4[i].os_virt_avail() = 0;
            pt5[i].os_virt_avail() = 0;
            pt6[i].os_virt_avail() = 1;
            pt7[i].os_virt_avail() = 1;
            pt8[i].os_virt_avail() = 1;
            pt9[i].os_virt_avail() = 1;
            pt10[i].os_virt_avail() = 1;
            pt11[i].os_virt_avail() = 1;
            pt12[i].os_virt_avail() = 1;
            pt13[i].os_virt_avail() = 1;
            pt14[i].os_virt_avail() = 1;
            pt15[i].os_virt_avail() = 1;
            pt16[i].os_virt_avail() = 1;
            pt17[i].os_virt_avail() = 1;
            pt18[i].os_virt_avail() = 1;
            pt19[i].os_virt_avail() = 1;
            pt20[i].os_virt_avail() = 1;
            pt21[i].os_virt_avail() = 1;
            pt22[i].os_virt_avail() = 1;
            pt23[i].os_virt_avail() = 1;
        }
        // Our stack is at pt3[511] /* number 6 (index 5) in 1st (index 0) pdt */
        {
            auto *stackph = allocate_pages(10);
            if (stackph == nullptr) {
                print(L"FATAL ERROR: Failed to allocate memory for stack");
                asm("ud2");
            }
            uint32_t stack_page{static_cast<uint32_t>(reinterpret_cast<phys_t>(stackph) / 0x1000)};
            print(L"Stack allocated at: ");
            print_u32(stack_page);
            print(L"-");
            print_u32(stack_page+10);
            print(L"\r\n");
            for (uint16_t i = 502; i < 512; i++) {
                pt3[i].os_virt_avail() = 0;
                pt3[i].writeable() = 1;
                pt3[i].present() = 1;
                pt3[i].execution_disabled() = 1;
                pt3[i].page_ppn() = stack_page + i;
                physpageMap->claim(stack_page + i);
            }
        }
        pt3[502].os_virt_start() = 1;
        pt3[502].present() = 0; // Crash barrier

        {
            uint32_t ph_page_start{static_cast<uint32_t>(reinterpret_cast<phys_t>(physpageStructsAddr) / 0x1000)};
            for (uint32_t i = 0; i < conf_pages; i++) {
                uint64_t vaddr{reinterpret_cast<uint64_t>(physpageStructsAddr) + (static_cast<uint64_t>(i) * 0x1000)};
                uint32_t ph_page_addr{ph_page_start + i};
                pageentr *pe = get_pageentr64(pml4t, vaddr);
                if (pe == nullptr) {
                    print(L"FATAL ERROR: Config virtual address out of loader mapping range\r\n");
                    asm("ud2");
                }
                pe->present() = 1;
                pe->writeable() = 1;
                pe->user_access() = 0;
                pe->write_through() = 0;
                pe->cache_disabled() = 0;
                pe->accessed() = 0;
                pe->dirty() = 0;
                pe->size() = 0;
                pe->global() = 1;
                pe->os_virt_avail() = 0;
                pe->page_ppn() = ph_page_addr;
                pe->os_virt_start() = 1;
                pe->execution_disabled() = 1;
                print(L"Confmap rw ");
                print_u64(vaddr);
                print(L" -> page:");
                print_u32(ph_page_addr);
                print(L"\r\n");
            }
        }

        {
            uint32_t ph_page_start{static_cast<uint32_t>(reinterpret_cast<phys_t>(phys_kernel) / 0x1000)};
            for (uint32_t i = 0; i < kernel_pages; i++) {
                uint64_t vaddr{kernel_vmem_start + (static_cast<uint64_t>(i) * 0x1000)};
                uint32_t ph_page_addr{ph_page_start + i};
                pageentr *pe = get_pageentr64(pml4t, vaddr);
                if (pe == nullptr) {
                    print(L"FATAL ERROR: Kernel virtual address out of loader mapping range\r\n");
                    asm("ud2");
                }
                pe->present() = 1;
                pe->writeable() = 0;
                pe->user_access() = 0;
                pe->write_through() = 0;
                pe->cache_disabled() = 0;
                pe->accessed() = 0;
                pe->dirty() = 0;
                pe->size() = 0;
                pe->global() = 1;
                pe->os_virt_avail() = 0;
                pe->page_ppn() = ph_page_addr;
                pe->os_virt_start() = 1;
                pe->execution_disabled() = 1;
                //print(L"Map rd ");
                //print_u64(vaddr);
                //print(L" -> page:");
                //print_u32(ph_page_addr);
                //print(L"\r\n");
            }
        }

        for (uint16_t i = 0; i < stageloader_header.e_phnum; i++) {
            const auto &ph = stageloader_header.get_program_entry(i);
            print(L"SLph ");
            print_u64(ph.p_offset);
            print(L" ");
            print_u64(ph.p_vaddr);
            print(L" ");
            print_u64(ph.p_filesz);
            print(L" ");
            print_u64(ph.p_memsz);
            print(L" ");
            print_u64(ph.p_flags);
            print(L"\r\n");

            if (ph.p_memsz > 0 && ph.p_type == PHT_LOAD) {
                phys_t phaddr = reinterpret_cast<phys_t>(phys_stageloader) + ph.p_offset;
                print(L"   -> ");
                print_u64(phaddr);
                print(L"\r\n");
                    /*{
                    uint32_t overrun = phdata & 4095;
                    if (overrun != 0) {
                        phdata += 4096 - overrun;
                    }
                }*/
                uint32_t vaddr = phaddr;
                uint32_t vaddr_end = vaddr + ph.p_memsz;
                uint32_t alignment_error{0};
                if ((phaddr & 4095) != 0 || (vaddr & 4095) != 0) {
                    alignment_error = phaddr & 4095;
                    if (alignment_error != (vaddr & 4095)) {
                        print(L"FATAL ERROR: Kernel exec not page aligned\r\n");
                        asm("ud2");
                    }
                    phaddr -= alignment_error;
                    vaddr -= alignment_error;
                }
                if (ph.p_filesz == ph.p_memsz) {
                    while (vaddr < vaddr_end) {
                        uint32_t page_ppn = phaddr / 4096;
                        pageentr *pe = get_pageentr64(pml4t, vaddr);

                        /*
                         * Default read only and non-exec
                         */
                        pe->writeable() = 0;
                        pe->execution_disabled() = 1;

                        pe->os_virt_avail() = 0;
                        pe->os_virt_start() = 1;
                        pe->page_ppn() = page_ppn;
                        pe->present() = 1;

                        vaddr += 4096;
                        phaddr += 4096;
                    }
                } else {
                    print(L"FATAL ERROR: No heap loading for stageloader\r\n");
                    asm("ud2");
                }
            }
        }

        for (uint16_t i = 0; i < stageloader_header.e_shnum; i++) {
            const auto &section = stageloader_header.get_section_entry(i);
            if (section.sh_addr != 0 && section.sh_size != 0) {
                phys_t phaddr = reinterpret_cast<phys_t>(phys_stageloader) + section.sh_offset;
                phys_t end_phaddr = phaddr + ((phys_t) section.sh_size);
                phaddr = phaddr & 0xFFFFF000;
                for (; phaddr < end_phaddr; phaddr += 0x1000) {
                    pageentr *pe = get_pageentr64(pml4t, phaddr);
                    if (section.sh_flags & SHF_WRITE) {
                        pe->writeable() = 1;
                    }
                    if (section.sh_flags & SHF_EXECINSTR) {
                        pe->execution_disabled() = 0;
                    }
                }
            }
        }
        print(L"Stageloader entrypoint: ");
        print_u64(stageloader_entrypoint_addr);
        print(L" -> ");
        stageloader_entrypoint_addr += reinterpret_cast<phys_t>(phys_stageloader);
        print_u64(stageloader_entrypoint_addr);
        print(L"\r\n");

        phys_t phdata = reinterpret_cast<phys_t>(phys_kernel) + kernel_size;
        for (uint16_t i = 0; i < elf64_header.e_phnum; i++) {
            const auto &ph = elf64_header.get_program_entry(i);
            print(L"ph ");
            print_u64(ph.p_offset);
            print(L" ");
            print_u64(ph.p_vaddr);
            print(L" ");
            print_u64(ph.p_filesz);
            print(L" ");
            print_u64(ph.p_memsz);
            print(L" ");
            print_u64(ph.p_flags);
            print(L"\r\n");

            if (ph.p_memsz > 0 && ph.p_type == PHT_LOAD) {
                phys_t phaddr = reinterpret_cast<phys_t>(phys_kernel) + ph.p_offset;
                {
                    uint32_t overrun = phdata & 4095;
                    if (overrun != 0) {
                        phdata += 4096 - overrun;
                    }
                }
                uint32_t vaddr = ph.p_vaddr;
                uint32_t vaddr_end = vaddr + ph.p_memsz;
                uint32_t alignment_error{0};
                if ((phaddr & 4095) != 0 || (vaddr & 4095) != 0) {
                    alignment_error = phaddr & 4095;
                    if (alignment_error != (vaddr & 4095)) {
                        print(L"FATAL ERROR: Kernel exec not page aligned\r\n");
                        asm("ud2");
                    }
                    phaddr -= alignment_error;
                    vaddr -= alignment_error;
                }
                if (ph.p_filesz == ph.p_memsz) {
                    while (vaddr < vaddr_end) {
                        uint32_t page_ppn = phaddr / 4096;
                        pageentr *pe = get_pageentr64(pml4t, vaddr);

                        /*
                         * Default read only and non-exec
                         */
                        pe->writeable() = 0;
                        pe->execution_disabled() = 1;

                        pe->os_virt_avail() = 0;
                        pe->os_virt_start() = 1;
                        pe->page_ppn() = page_ppn;
                        pe->present() = 1;

                        vaddr += 4096;
                        phaddr += 4096;
                    }
                } else {
                    uint32_t cp = ph.p_filesz;
                    while (vaddr < vaddr_end) {
                        void *phptr;
                        uint32_t page_ppn;
                        phptr = allocate_pages(1);
                        if (phptr == nullptr) {
                            print(L"FATAL ERROR: Failed to allocate data page");
                            asm("ud2");
                        }
                        page_ppn = static_cast<uint32_t>(reinterpret_cast<phys_t>(phptr) / 0x1000);
                        physpageMap->claim(page_ppn);
                        pageentr *pe = get_pageentr64(pml4t, vaddr);
                        pe->os_virt_avail() = 0;
                        pe->os_virt_start() = 1;

                        /*
                         * Default read-only and non-exec
                         */
                        pe->writeable() = 0;
                        pe->execution_disabled() = 1;

                        pe->page_ppn() = page_ppn;
                        pe->present() = 1;

                        {
                            uint8_t *z = (uint8_t *) phptr;
                            int i = 0;
                            /*
                             * Write data from ELF file if within the file size.
                             */
                            for (; cp > 0 && i < 4096; i++) {
                                z[i] = *((uint8_t *) phaddr);
                                ++phaddr;
                                --cp;
                            }
                            /*
                             * Zero if outside file size.
                             */
                            for (; i < 4096; i++) {
                                z[i] = 0;
                            }
                        }
                        vaddr += 4096;
                    }
                }
            }
        }

        for (uint16_t i = 0; i < elf64_header.e_shnum; i++) {
            const auto &section = elf64_header.get_section_entry(i);
            if (section.sh_addr != 0 && section.sh_size != 0) {
                uint32_t vaddr = (uint32_t) section.sh_addr;
                uint32_t end_vaddr = vaddr + ((uint32_t) section.sh_size);
                vaddr = vaddr & 0xFFFFF000;
                for (; vaddr < end_vaddr; vaddr += 0x1000) {
                    pageentr *pe = get_pageentr64(pml4t, vaddr);
                    if (section.sh_flags & SHF_WRITE) {
                        pe->writeable() = 1;
                    }
                    if (section.sh_flags & SHF_EXECINSTR) {
                        pe->execution_disabled() = 0;
                    }
                }
            }
        }

        {
            const auto *rela_dyn = elf64_header.get_rela_dyn_section();
            if (rela_dyn != nullptr) {
                ELF64_rela_dyn *rela_dyns = (ELF64_rela_dyn *) (void *) (((uint8_t *) &(elf64_header.start)) +rela_dyn->sh_offset);
                /* X - May not handle very big images (in the astronomical range >4G->1TB) due to overflow */
                static_assert((3 << 3) == sizeof(*rela_dyns));
                auto rela_dyn_count = (uint32_t) (rela_dyn->sh_size >> 3);
                rela_dyn_count = rela_dyn_count / 3;
                for (uint32_t i = 0; i < rela_dyn_count; i++) {
                    switch (rela_dyns[i].rela_type) {
                        case R_X86_64_RELATIVE: {
                            if (rela_dyns[i].sym_index != 0) {
                                asm("ud2");
                            }
                            pageentr *pe = get_pageentr64(pml4t, rela_dyns[i].offset & ~((uint64_t) 0xFFF));
                            if (pe == nullptr) {
                                asm("ud2");
                            }
                            uint64_t phys = static_cast<uint64_t>(pe->page_ppn()) << 12;
                            phys += rela_dyns[i].offset & 0xFFF;
                            *((uint64_t *) phys) += rela_dyns[i].addendum;
                        }
                            break;
                    }
                }
            }
        }

        int stack_pages = 7;
        ++stack_pages;
        phys_t stack_addr;
        phys_t stack_size{static_cast<phys_t>(stack_pages)};
        stack_size *= 0x1000;
        void *ptr = allocate_config_pages(stack_pages);
        if (ptr == nullptr) {
            print(L"FATAL ERROR: Unable to allocate CPU-boot stack");
            asm("ud2");
        }
        {
            stack_addr = reinterpret_cast<phys_t>(ptr);
            uint32_t stack_pageaddr{static_cast<uint32_t>(stack_addr / 0x1000)};
            for (int i = 0; i < stack_pages; i++) {
                pageentr *pe = get_pageentr64(pml4t, stack_addr + (static_cast<phys_t>(i) * 0x1000));
                if (pe == nullptr) {
                    print(L"FATAL ERROR: Unable to map stack pages");
                }
                pe->os_virt_avail() = 0;
                pe->os_virt_start() = i == 0 ? 1 : 0;
                pe->page_ppn() = stack_pageaddr + i;
                if (i > 0) {
                    physpageMap->claim(stack_pageaddr + i);
                }
                pe->writeable() = 1;
                pe->present() = i > 0 ? 1 : 0;
                pe->accessed() = 0;
                pe->execution_disabled() = 1;
            }
        }
        stack_addr += stack_size;

        GDT_table<GDT_SIZE + (LDT_DESC_SIZE * 2)> &init_gdt64 = *((GDT_table<GDT_SIZE + (LDT_DESC_SIZE * 2)> *) gdtAddr);
        uint64_t gdt_ptr;
        {
            static_assert(sizeof(init_gdt64) <= GDT_MAX_SIZE);
            memset(&init_gdt64, 0, sizeof(init_gdt64));
            init_gdt64[0] = GDT(0, 0x1FFFF, 0, 0);
            init_gdt64[1] = GDT(0, 0xF0000, 0xA, 0x9A);
            init_gdt64[2] = GDT(0, 0, 0, 0x92);
            print(L"GDT:");
            uint64_t *gdt64 = (uint64_t *) &init_gdt64;
            gdt_ptr = init_gdt64.pointer();
            print_u64(gdt64[0]);
            print(L" ");
            print_u64(gdt64[1]);
            print(L" ");
            print_u64(gdt64[2]);
            print(L"\r\n");
            print(L"GDT pointer: ");
            print_u64(gdt_ptr);
            print(L"\r\n");
        }

        print(L"Go for launch!\r\n");

        uintptr_t memoryMapSize{0};
        uintptr_t memoryMapDescriptorSize{0};
        efi_system_table_ptr->boot_services->get_memory_map(&memoryMapSize, nullptr, nullptr, &memoryMapDescriptorSize, nullptr);
        print(L"Mem map size query result ");
        print_u64(memoryMapSize);
        print(L" desc ");
        print_u64(memoryMapDescriptorSize);
        print(L"\r\n");
        void *memoryMap;
        uintptr_t memoryMapKey;
        {
            memoryMapSize += memoryMapDescriptorSize * 2;
            auto pages = memoryMapSize / 0x1000;
            if ((memoryMapSize % 0x1000) > 0) {
                ++pages;
            }
            memoryMap = allocate_pages(pages);
            if (memoryMap == nullptr) {
                print(L"FATAL ERROR: Could not allocate memory for memory map\r\n");
                asm("ud2");
            }
            memoryMapSize = pages;
            memoryMapSize *= 0x1000;
            uint32_t version;
            print(L"Try get map size=");
            print_u64(memoryMapSize);
            print(L" bytes\r\n");
            if (efi_system_table_ptr->boot_services->get_memory_map(&memoryMapSize, memoryMap, &memoryMapKey, &memoryMapDescriptorSize, &version) == 0) {
                //print(L"Map ");
                //print_ptr(memoryMap);
                //print(L"size=");
                //print_u64(memoryMapSize);
                //print(L" bytes\r\n");
            } else {
                print(L"OOps size=");
                print_u64(memoryMapSize);
                print(L" pages=");
                print_u32(pages);
                print(L"\r\n");
                print(L"FATAL ERROR: Failed to get memory map\r\n");
                asm("ud2");
            }
        }
        for (phys_t i = reinterpret_cast<phys_t>(memoryMap); i < reinterpret_cast<phys_t>(memoryMap) + memoryMapSize; i += 0x1000) {
            physpageMap->claim(i / 0x1000);
        }
        /*print(L"Memory map size ");
        print_u64(memoryMapSize);
        print(L" descriptor size ");
        print_u64(memoryMapDescriptorSize);
        phys_t memoryMapSlots = memoryMapSize / memoryMapDescriptorSize;
        print(L" -> slots ");
        print_u64(memoryMapSlots);
        print(L"\r\n");
        for (phys_t i = 0; i < memoryMapSlots; i++) {
            efi_memory_descriptor *memoryMapDescriptor = reinterpret_cast<efi_memory_descriptor *>(reinterpret_cast<phys_t>(memoryMap) + (memoryMapDescriptorSize * i));
            if (memoryMapDescriptor->number_of_pages == 0) {
                continue;
            }
            print(L"Mem ");
            print_ptr(memoryMapDescriptor);
            print(L" type ");
            print_u32(memoryMapDescriptor->type);
            print(L" phys ");
            print_u64(memoryMapDescriptor->physical_start);
            print(L" virt ");
            print_u64(memoryMapDescriptor->virtual_start);
            print(L" count ");
            print_u64(memoryMapDescriptor->number_of_pages);
            print(L" attr ");
            print_u64(memoryMapDescriptor->attribute);
            print(L"\r\n");
        }*/
        efi_system_table_ptr->boot_services->exit_boot_services(ImageHandle, memoryMapKey);

        phys_t rsp = stack_addr - sizeof(UefiStageContext);
        {
            phys_t alignment_error = rsp & 7;
            rsp -= alignment_error;
        }

        UefiStageContext *context = new (reinterpret_cast<void *>(rsp)) UefiStageContext();
        context->vmem_root = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1000);
        context->physpage_map = reinterpret_cast<uint64_t>(physpageMap);
        context->entrypoint_addr = entrypoint_addr;
        context->pml4t_addr = reinterpret_cast<uint64_t>(&pml4t);
        context->efi_memory_map_page_aligned_plus_descriptor_size = reinterpret_cast<uint64_t>(memoryMap) + memoryMapDescriptorSize;
        context->efi_memory_map_size = memoryMapSize;
        uint64_t context_ptr{reinterpret_cast<uint64_t>(context)};
        rsp -= 8;
        {
            phys_t alignment_error = (rsp - 8) & 15;
            rsp -= alignment_error;
        }
        *(reinterpret_cast<uint64_t *>(rsp)) = 0;

        asm("lgdt (%0)" :: "r"(gdt_ptr));
        uint64_t jmpAddr[2] = {stageloader_entrypoint_addr, 8};
        uint64_t jmpAddrPtr = reinterpret_cast<uint64_t>(&(jmpAddr[0]));
        asm("mov %0, %%rax; mov %1, %%rdi; mov %2, %%rsp; ljmpq *(%%rax)" :: "r"(jmpAddrPtr), "r"(context_ptr), "r"(rsp) : "%rax", "%rdi", "%rsp");
    }
}