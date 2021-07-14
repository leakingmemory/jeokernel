//
// Created by sigsegv on 20.04.2021.
//

#ifndef JEOKERNEL_PAGEALLOC_H
#define JEOKERNEL_PAGEALLOC_H

#ifndef UNIT_TESTING

#include <stdint.h>
#include <pagetable.h>

#endif

uint64_t vpagealloc(uint64_t size);
void pmemcounts();
uint64_t ppagealloc(uint64_t size);
uint32_t ppagealloc32(uint32_t size);
uint64_t vpagefree(uint64_t addr);
void ppagefree(uint64_t addr, uint64_t size);

uint64_t pv_fix_pagealloc(uint64_t size);
uint64_t pv_fix_pagefree(uint64_t addr);

uint64_t alloc_stack(uint64_t size);
void free_stack(uint64_t addr);

void reload_pagetables();

void *pagealloc(uint64_t size);
void pagefree(void *vaddr);

#endif //JEOKERNEL_PAGEALLOC_H
