//
// Created by sigsegv on 4/8/26.
//

#include <uefi.h>
#include <new>

#include <pagetable.h>
#include <physpagemap.h>
#include <elf.h>
#include <elf_impl.h>

typedef void* EFI_HANDLE;

static efi_system_table *efi_system_table_ptr;

static int print(const wchar_t *str) {
    return efi_system_table_ptr->con_out->output_string(efi_system_table_ptr->con_out, str);
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
    extern const void *wrapped_binary_end;

    void EFIAPI efi_main(EFI_HANDLE ImageHandle, void *ptr_SystemTable) {
        efi_system_table_ptr = reinterpret_cast<efi_system_table *>(ptr_SystemTable);
        uint8_t *kernel_src = reinterpret_cast<uint8_t *>(&wrapped_binary);
        print(L"Kernel embedded binary: ");
        print_ptr(kernel_src);
        print(L"\r\nKernel size: ");
        auto kernel_size = reinterpret_cast<uintptr_t>(&wrapped_binary_end) - reinterpret_cast<uintptr_t>(kernel_src);
        print_i32(static_cast<uint32_t>(kernel_size));
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
        ELF kernel{(void *) kernel_src, (void *) &wrapped_binary_end};
        if (!kernel.is_valid()) {
            print(L"FATAL ERROR: Kernel binary is not valid");
            asm("ud2");
        }
        const auto &elf64_header = kernel.get_elf64_header();
        print(L"Physpages for memory management structs: ");
        auto *physpageStructsAddr = (uint8_t *) allocate_pages(0x21);
        print_ptr(physpageStructsAddr);
        print(L"\r\n");
        auto *physpageMap = new (physpageStructsAddr) PhyspageMap();

        for (uint32_t i = 0; i < kernel_pages; i++) {
            physpageMap->claim((reinterpret_cast<uint64_t>(phys_kernel) / 0x1000) + i);
        }
        for (int i = 0; i < 0x21; i++) {
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

        pml4t[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x2000) / 4096;
        pml4t[0].writeable() = 1;
        pml4t[0].present() = 1;
        pml4t[0].os_virt_avail() = 1;
        pdpt[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x3000) / 4096;
        pdpt[0].writeable() = 1;
        pdpt[0].present() = 1;
        pdpt[0].os_virt_avail() = 1;
        pdpt[3].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x4000) / 4096;
        pdpt[3].writeable() = 1;
        pdpt[3].present() = 1;
        pdpt[3].os_virt_avail() = 1;
        pdt0[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xa000) / 4096;
        pdt0[0].writeable() = 1;
        pdt0[0].present() = 1;
        pdt0[0].os_virt_avail() = 1;
        pdt0[1].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xb000) / 4096;
        pdt0[1].writeable() = 1;
        pdt0[1].present() = 1;
        pdt0[1].os_virt_avail() = 1;
        /* stack area for bootloader */
        pdt0[5].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xc000) / 4096;
        pdt0[5].writeable() = 1;
        pdt0[5].present() = 1;
        pdt0[5].os_virt_avail() = 1;
        /* ** */
        pdt[0].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xd000) / 4096;
        pdt[0].writeable() = 1;
        pdt[0].present() = 1;
        pdt[0].os_virt_avail() = 0;
        pdt[1].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xe000) / 4096;
        pdt[1].writeable() = 1;
        pdt[1].present() = 1;
        pdt[1].os_virt_avail() = 1;
        pdt[2].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0xf000) / 4096;
        pdt[2].writeable() = 1;
        pdt[2].present() = 1;
        pdt[2].os_virt_avail() = 1;
        pdt[3].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x10000) / 4096;
        pdt[3].writeable() = 1;
        pdt[3].present() = 1;
        pdt[3].os_virt_avail() = 1;
        pdt[4].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x11000) / 4096;
        pdt[4].writeable() = 1;
        pdt[4].present() = 1;
        pdt[4].os_virt_avail() = 1;
        pdt[5].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x12000) / 4096;
        pdt[5].writeable() = 1;
        pdt[5].present() = 1;
        pdt[5].os_virt_avail() = 1;
        pdt[6].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x13000) / 4096;
        pdt[6].writeable() = 1;
        pdt[6].present() = 1;
        pdt[6].os_virt_avail() = 1;
        pdt[7].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x14000) / 4096;
        pdt[7].writeable() = 1;
        pdt[7].present() = 1;
        pdt[7].os_virt_avail() = 1;
        pdt[8].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x15000) / 4096;
        pdt[8].writeable() = 1;
        pdt[8].present() = 1;
        pdt[8].os_virt_avail() = 1;
        pdt[9].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x16000) / 4096;
        pdt[9].writeable() = 1;
        pdt[9].present() = 1;
        pdt[9].os_virt_avail() = 1;
        pdt[10].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x17000) / 4096;
        pdt[10].writeable() = 1;
        pdt[10].present() = 1;
        pdt[10].os_virt_avail() = 1;
        pdt[11].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x18000) / 4096;
        pdt[11].writeable() = 1;
        pdt[11].present() = 1;
        pdt[11].os_virt_avail() = 1;
        pdt[12].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x19000) / 4096;
        pdt[12].writeable() = 1;
        pdt[12].present() = 1;
        pdt[12].os_virt_avail() = 1;
        pdt[13].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1a000) / 4096;
        pdt[13].writeable() = 1;
        pdt[13].present() = 1;
        pdt[13].os_virt_avail() = 1;
        pdt[14].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1b000) / 4096;
        pdt[14].writeable() = 1;
        pdt[14].present() = 1;
        pdt[14].os_virt_avail() = 1;
        pdt[15].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1c000) / 4096;
        pdt[15].writeable() = 1;
        pdt[15].present() = 1;
        pdt[15].os_virt_avail() = 1;
        pdt[16].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1d000) / 4096;
        pdt[16].writeable() = 1;
        pdt[16].present() = 1;
        pdt[16].os_virt_avail() = 1;
        pdt[17].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1e000) / 4096;
        pdt[17].writeable() = 1;
        pdt[17].present() = 1;
        pdt[17].os_virt_avail() = 1;
        pdt[18].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x1f000) / 4096;
        pdt[18].writeable() = 1;
        pdt[18].present() = 1;
        pdt[18].os_virt_avail() = 1;
        pdt[19].page_ppn() = reinterpret_cast<uint64_t>(physpageStructsAddr + 0x20000) / 4096;
        pdt[19].writeable() = 1;
        pdt[19].present() = 1;
        pdt[19].os_virt_avail() = 1;

        while (1) {
            asm("hlt");
        }
    }
}