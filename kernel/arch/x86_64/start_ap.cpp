//
// Created by sigsegv on 05.05.2021.
//

#include "start_ap.h"
#include <core/vmem.h>
#include <cstring>
#include <pagetable.h>
#include <pagealloc.h>
#include <concurrency/raw_spinlock.h>

#include "GlobalDescriptorTable.h"

static raw_spinlock *ap_start_lock = nullptr;

raw_spinlock *get_ap_start_lock() {
    return ap_start_lock;
}

extern "C" {
void ap_started();
}

const uint32_t *install_ap_bootstrap(GlobalDescriptorTable &gdt) {
    ap_start_lock = new raw_spinlock();
    vmem vm{4096};
    vm.page(0).rwmap(0x8000, true);
    uintptr_t &stackptr_ref = *((uintptr_t *) (void *) (((uint8_t *) vm.pointer()) + 0x150));
    uintptr_t stackptr = stackptr_ref;
    uint8_t *bootstrap_location = (uint8_t *) vm.pointer();
    uint8_t *bootstrap_ptr = (uint8_t *) (void *) ap_trampoline;
    uint8_t *bootstrap_end = (uint8_t *) (void *) ap_trampoline_end;
    while (bootstrap_ptr != bootstrap_end) {
        *bootstrap_location = *bootstrap_ptr;
        ++bootstrap_ptr;
        ++bootstrap_location;
    }
    *((uintptr_t *) (void *) (((uint8_t *) vm.pointer()) + 0x140)) = reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(ap_started));
    stackptr_ref = stackptr;
    *((uint32_t *) (void *) (((uint8_t *) vm.pointer()) + 0x160)) = static_cast<uint32_t>(get_init_pml4t() + 0x4000);
    memcpy(reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(vm.pointer()) + 0x170), gdt.pointer(), 12);
    std::optional<pageentr> pe = get_pageentr(KERNEL_MEMORY_OFFSET + 0x8000);
    pe->execution_disabled() = 0;
    pe->writeable() = 0;
    pe->present() = 1;
    update_pageentr(KERNEL_MEMORY_OFFSET + 0x8000, *pe);
    reload_pagetables();
    return (const uint32_t *) (KERNEL_MEMORY_OFFSET + 0x8FF0);
}