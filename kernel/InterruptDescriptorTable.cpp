//
// Created by sigsegv on 25.04.2021.
//

#include <new>
#include <string.h>
#include <pagealloc.h>
#include <klogger.h>
#include "InterruptDescriptorTable.h"

InterruptDescriptorTable::InterruptDescriptorTable() {
    idt_table = new ((void *) pv_fix_pagealloc(sizeof(*idt_table))) IDTTable<256>();
    if (idt_table == nullptr) {
        wild_panic("Failed to allocate IDT");
    }
}

InterruptDescriptorTable::~InterruptDescriptorTable() {
    if (idt_table != nullptr) {
        idt_table->~IDTTable();
        pv_fix_pagefree((uint64_t) idt_table);
        idt_table = nullptr;
    }
}

void InterruptDescriptorTable::install() {
    uint64_t pointer = (uint64_t) &(idt_table->pointer);
    asm("mov %0, %%rax; lidt (%%rax)":: "r"(pointer) : "%rax");
}
