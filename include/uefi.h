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

struct efi_guid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];

    constexpr bool operator==(const efi_guid &other) const {
        if (data1 == other.data1 && data2 == other.data2 && data3 == other.data3) {
            for (int i = 0; i < 8; i++) {
                if (data4[i] != other.data4[i]) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
};

constexpr efi_guid GOP_PROTOCOL_ID = {
    0x9042a9de,
    0x23dc,
    0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};

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
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    EFI_STATUS (EFIAPI *locate_protocol)(efi_guid *guid, void *registration, void **interface);
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

struct efi_memory_descriptor {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint32_t attribute;
};

enum efi_pixel_format {
    dword_RGBR = 0,
    dword_BGRR = 1,
    pixel_bit_mask = 2,
    blt_only = 3
};

struct efi_graphics_output_mode_info {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    uint32_t pixel_information;
    uint32_t pixels_per_scan_line;
};

struct efi_graphics_output_mode {
    uint32_t max_mode;
    uint32_t mode;
    const efi_graphics_output_mode_info *info;
    uintptr_t size_of_info;
    phys_t frame_buffer_base;
    uintptr_t size_of_frame_buffer;
};

struct efi_gop_protocol {
    EFI_STATUS (EFIAPI *query_mode)(efi_gop_protocol *, uint32_t mode, uintptr_t *sizeofinfo, const efi_graphics_output_mode_info **info);
    EFI_STATUS (EFIAPI *set_mode)(efi_gop_protocol *, uint32_t);
    void *blt;
    const efi_graphics_output_mode *mode;
};

#endif //JEOKERNEL_UEFI_H
