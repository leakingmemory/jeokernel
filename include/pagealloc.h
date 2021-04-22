//
// Created by sigsegv on 20.04.2021.
//

#ifndef JEOKERNEL_PAGEALLOC_H
#define JEOKERNEL_PAGEALLOC_H

#include <stdint.h>
#include <pagetable.h>

uint64_t vpagealloc(uint64_t size);
uint64_t ppagealloc(uint64_t size);
uint64_t vpagefree(uint64_t addr);
void ppagefree(uint64_t addr, uint64_t size);

void *pagealloc(uint64_t size);
void pagefree(void *vaddr);

#endif //JEOKERNEL_PAGEALLOC_H
