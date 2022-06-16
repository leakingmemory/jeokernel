//
// Created by sigsegv on 20.04.2021.
//

#ifndef JEOKERNEL_PAGEALLOC_H
#define JEOKERNEL_PAGEALLOC_H

#ifndef UNIT_TESTING

#include <memory>
#include <stdint.h>
#include <pagetable.h>

#endif

#define PMLT4_USERSPACE_START 0x10

class ApStartup;
class vmem;

struct VPerCpuPagetables {
    std::shared_ptr<vmem> vm;
    pagetable *tables;
    int numTables;
    uintptr_t pointer;
};

pagetable &get_root_pagetable();
uintptr_t vpagealloc(uintptr_t size);
uintptr_t vpagealloc32(uintptr_t size);
VPerCpuPagetables vpercpuallocpagetable();
VPerCpuPagetables vpercpuallocpagetable32();
uint32_t vpagetablecount();
void pmemcounts();
phys_t ppagealloc(uintptr_t size);
phys_t ppagealloc32(uint32_t size);
uintptr_t vpagefree(uintptr_t addr);
uintptr_t vpagesize(uintptr_t addr);
void ppagefree(phys_t addr, uintptr_t size);

uintptr_t pv_fix_pagealloc(uintptr_t size);
uintptr_t pv_fix_pagefree(uintptr_t addr);

uintptr_t alloc_stack(uintptr_t size);
void free_stack(uintptr_t addr);

void reload_pagetables();

void *pagealloc(uintptr_t size);
void pagefree(void *vaddr);

void vmem_switch_to_multicpu(ApStartup *apStartup, int numCpus);
void vmem_set_per_cpu_pagetables();

void setup_pvpage_stats();

#endif //JEOKERNEL_PAGEALLOC_H
