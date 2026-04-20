//
// Created by sigsegv on 4/8/26.
//

#ifndef JEOKERNEL_UEFI_H
#define JEOKERNEL_UEFI_H

#include "cstdint"

#ifndef EFIAPI
#ifdef _MSVC_LANG
#define EFIAPI __cdecl
#else
#define EFIAPI __attribute__((cdecl))
#endif
#endif

typedef int EFI_STATUS;

struct efi_table_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
};

struct efi_runtime_services {
    efi_table_header header;
    void *get_time;
    void *set_time;
    void *get_wakeup_time;
    void *set_wakeup_time;
    void *set_virtual_address_map;
    void *convert_pointer;
    void *get_next_monotonic_count;
    void *reset_system;
    void *update_capsule;
    void *query_capsule_capabilities;
    void *query_variable_info;
};

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EfiAllocateType;

struct efi_boot_services {
    efi_table_header header;
    void *raise_tpl;
    void *restore_tpl;
    EFI_STATUS (EFIAPI *allocate_pages)(EfiAllocateType type, uint64_t memory_type, uint64_t pages, void **memory);
    EFI_STATUS (EFIAPI *free_pages)(void *addr, uintptr_t pages);
    EFI_STATUS (EFIAPI *get_memory_map)(uintptr_t *MemoryMapSize, void *MemoryMap, uintptr_t *MapKey,uintptr_t *DescriptorSize, uint32_t *DescriptorVersion);
    void *allocate_pool;
    void *free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    void *handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    EFI_STATUS (EFIAPI *exit_boot_services)(void *ImageHandle, uintptr_t MapKey);
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    void *locate_protocol;
    void *install_multiple_protocol_interfaces;
    void *uninstall_multiple_protocol_interfaces;
    void *calculate_checksum;
    void *copy_memory;
    void *set_memory;
    void *create_event_ex;
};

struct efi_table_header_ptr {
    uint64_t vendor_guid_1;
    uint64_t vendor_guid_2;
    void *table;
};

struct efi_simple_text_output {
    void *reset;
    EFI_STATUS (EFIAPI *output_string)(efi_simple_text_output *thptr, const wchar_t *string);
    void *test_string;
    void *query_mode;
    void *set_mode;
    void *set_attribute;
    void *clear_screen;
    void *set_cursor_position;
    void *enable_cursor;
    void *mode;
};

struct efi_system_table {
    efi_table_header header;
    char16_t *firmware_vendor_ptr;
    uint32_t firmware_revision;
    void *con_in_handle;
    void *con_in;
    void *con_out_handle;
    efi_simple_text_output *con_out;
    void *stderr_handle;
    void *stderr;
    efi_runtime_services *runtime_services;
    efi_boot_services *boot_services;
    uint32_t num_table_entries;
    efi_table_header_ptr *table_entries[];
};

#endif //JEOKERNEL_UEFI_H
