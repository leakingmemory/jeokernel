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

#define USERSPACE_LOW_END   3
#define PMLT4_USERSPACE_HIGH_START 0x10

#define KERNEL_MEMORY_OFFSET    ((uintptr_t) USERSPACE_LOW_END*1024*1024*1024)

static_assert(USERSPACE_LOW_END > 0 && USERSPACE_LOW_END < 512);
static_assert(PMLT4_USERSPACE_HIGH_START > 0);

class ApStartup;
class vmem;

struct VPerCpuPagetables {
    std::shared_ptr<vmem> vm;
    pagetable *tables;
    int numTables;
    uintptr_t pointer;
};

void relocate_kernel_vmemory();
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

uintptr_t pv_fixp1g_pagealloc(uintptr_t size);
uintptr_t pv_fixp1g_pagefree(uintptr_t addr);

uintptr_t alloc_stack(uintptr_t size);
void free_stack(uintptr_t addr);

void reload_pagetables();

void *pagealloc_phys32(uintptr_t size);
void *pagealloc32(uintptr_t size);
void *pagealloc(uintptr_t size);
void pagefree(void *vaddr);

void vmem_switch_to_multicpu(ApStartup *apStartup, int numCpus);
void vmem_set_per_cpu_pagetables();

void setup_pvpage_stats();

#endif //JEOKERNEL_PAGEALLOC_H
