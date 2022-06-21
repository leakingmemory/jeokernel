//
// Created by sigsegv on 25.04.2021.
//

#include <new>
#include <string.h>
#include <pagealloc.h>
#include <klogger.h>
#include "InterruptDescriptorTable.h"

InterruptDescriptorTable::InterruptDescriptorTable() {
    physptr = ppagealloc(sizeof(*idt_table));
    if (physptr == 0) {
        wild_panic("Failed to allocate IDT <phys>");
    }
    idt_table = (IDTTable<256> *) vpagealloc(sizeof(*idt_table));
    if (idt_table == nullptr) {
        wild_panic("Failed to allocate IDT");
    }
    for (uintptr_t offset = 0; offset < sizeof(*idt_table); offset += 4096) {
        update_pageentr(((uintptr_t) idt_table) + offset, [this, offset] (pageentr &pe) {
            pe.page_ppn = (physptr + offset) >> 12;
            pe.present = 1;
            pe.writeable = 1;
            pe.execution_disabled = 1;
            pe.user_access = 0;
            pe.write_through = 0;
            pe.cache_disabled = 0;
        });
    }
    idt_table = new ((void *) idt_table) IDTTable<256>();
}

InterruptDescriptorTable::~InterruptDescriptorTable() {
    if (idt_table != nullptr) {
        idt_table->~IDTTable();
        vpagefree((uint64_t) idt_table);
        ppagefree(physptr, sizeof(*idt_table));
        idt_table = nullptr;
    }
}

void InterruptDescriptorTable::install() {
    uintptr_t pointer = (uintptr_t) &(idt_table->pointer);
    asm("mov %0, %%rax; lidt (%%rax)":: "r"(pointer) : "%rax");
}
