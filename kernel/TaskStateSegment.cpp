//
// Created by sigsegv on 25.04.2021.
//

#include "TaskStateSegment.h"
#include <pagealloc.h>
#include <new>

TaskStateSegment::TaskStateSegment() : tss_entry(new ((void *) pv_fix_pagealloc(sizeof(*tss_entry))) TSS_entry_with_iobmap()) {
    if (tss_entry == nullptr) {
        wild_panic("pagealloc ran out of good stuff");
    }
    if (((uint64_t) tss_entry) > 0xFFFFFFFF) {
        wild_panic("TSS address to high (32bit avail)");
    }
}

TaskStateSegment::~TaskStateSegment() {
    if (tss_entry != nullptr) {
        tss_entry->~TSS_entry_with_iobmap();
        pv_fix_pagefree((uint64_t) tss_entry);
        tss_entry = nullptr;
    }
}

int TaskStateSegment::install(GlobalDescriptorTable &gdt, int cpu_n) {
    int sel_idx = TSS_GD(cpu_n);
    GDT &gdt_e = gdt.get_descriptor(sel_idx);
    gdt_e.set_base((uint32_t) ((uint64_t) tss_entry));
    gdt_e.set_limit(sizeof(*tss_entry));
    gdt_e.type = 0x89;
    gdt_e.granularity = 0x4;
    return sel_idx;
}

int TaskStateSegment::install_bsp(GlobalDescriptorTable &gdt) {
    int sel_idx = BSP_TSS_GD;
    GDT &gdt_e = gdt.get_descriptor(sel_idx);
    gdt_e.set_base((uint32_t) ((uint64_t) tss_entry));
    gdt_e.set_limit(sizeof(*tss_entry));
    gdt_e.type = 0x89;
    gdt_e.granularity = 0x4;
    return sel_idx;
}

int TaskStateSegment::install_cpu(GlobalDescriptorTable &gdt, int cpu_n) {
    int sel_idx = TSS_GD(cpu_n);

    uint16_t selector = gdt.get_selector(sel_idx);

    asm("mov %0, %%ax; ltr %%ax" :: "r"(selector) : "%ax");

    return sel_idx;
}

int TaskStateSegment::install_bsp_cpu(GlobalDescriptorTable &gdt) {
    int sel_idx = BSP_TSS_GD;

    uint16_t selector = gdt.get_selector(sel_idx);

    asm("mov %0, %%ax; ltr %%ax" :: "r"(selector) : "%ax");

    return sel_idx;
}
