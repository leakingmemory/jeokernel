//
// Created by sigsegv on 4/8/26.
//

#include <uefi.h>

typedef void* EFI_HANDLE;

extern "C" {

    extern const void *wrapped_binary;
    extern const void *wrapped_binary_end;

    void EFIAPI efi_main(EFI_HANDLE ImageHandle, void *ptr_SystemTable) {
        auto *system_table = reinterpret_cast<efi_system_table *>(ptr_SystemTable);
        system_table->con_out->output_string(system_table->con_out, L"Hello, world!\n");
        while (1) {
            asm("hlt");
        }
    }
}