//
// Created by sigsegv on 3/16/22.
//

#ifndef JEOKERNEL_ARCH_X86_64_BOOTSTRAP_H
#define JEOKERNEL_ARCH_X86_64_BOOTSTRAP_H

#include <stdint.h>

struct Stage1Data;
struct MultibootInfoHeader;

// Boot state captured from the loader's Stage1Data by start_m64() (defined in
// arch/x86_64/bootstrap.cpp) and consumed by the rest of stage 1 init in
// m64-stage1.cpp.
extern const MultibootInfoHeader *multiboot_info;
extern uint32_t uefiMemoryMapPage;
extern uint32_t uefiMemoryMapDescrSize;
extern uint32_t uefiMemoryMapNumDescr;
extern uint32_t loaderGdtAddr;
extern uint32_t efi_horiz;
extern uint32_t efi_vert;
extern uint32_t efi_pixel_format;
extern uint32_t kernel_phys;
extern uint32_t kernel_size;
extern uint64_t efi_framebuffer;
extern uint64_t efi_framebuffer_size;
extern uint64_t efi_rsdp_ptr;

// Stage 1 entry point, called from arch/x86_64/bootstrap.S once the CPU is in
// 64-bit mode. It captures Stage1Data into the globals above, brings up the
// physical page map and the simplest malloc, switches to a real stack and
// jumps to secondboot.
extern "C" void start_m64(Stage1Data *stage1Data, uint64_t stackptr);

#endif //JEOKERNEL_ARCH_X86_64_BOOTSTRAP_H
