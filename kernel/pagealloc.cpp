//
// Created by sigsegv on 20.04.2021.
//

#include <pagealloc.h>
#include <klogger.h>
#include <concurrency/hw_spinlock.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include <sstream>
#include <iostream>
#include <stats/statistics_root.h>
#include <physpagemap.h>
#include "ApStartup.h"

#define DEBUG_PALLOC_FAILURE

static long long int total_ppages = 0;
static long long int total_vpages = 0;
static long long int allocated_ppages = 0;
static long long int allocated_vpages = 0;

static ApStartup *apStartup = nullptr;
static pagetable **per_cpu_pagetables = nullptr;
static int v_num_cpus = 0;
static bool is_v_multicpu = false;

#define _get_pml4t_this_cpu()  (is_v_multicpu ? *((pagetable *) (void *) (((phys_t) per_cpu_pagetables[apStartup->GetCpuNum()]) + get_pagetable_virt_offset())) : (*((pagetable *) (get_pagetable_virt_offset() + 0x1000))))
#define _get_pml4t_cpu0()  (is_v_multicpu ? *((pagetable *) (void *) (((phys_t) per_cpu_pagetables[0]) + get_pagetable_virt_offset())) : (*((pagetable *) (get_pagetable_virt_offset() + 0x1000))))
static phys_t ppagealloc_locked(uintptr_t size);

pagetable &get_root_pagetable() {
    return _get_pml4t_this_cpu();
}

void relocate_kernel_vmemory() {
    /* copy the initial pagetable for starting up APs - needs some of the 16/32bit memory fixed mappings */
    memcpy((void *) 0x5000, (void *) 0x1000, 0x1000);
    memcpy((void *) 0x6000, (void *) 0x2000, 0x1000);
    memcpy((void *) 0x7000, (void *) 0x3000, 0x1000);
    (*((pagetable *) 0x5000))[0].page_ppn = 0x6000 / 0x1000;
    (*((pagetable *) 0x6000))[0].page_ppn = 0x7000 / 0x1000;

    auto pmlt4 = _get_pml4t_cpu0();
    auto &pdtp = pmlt4[0].get_subtable();
    uint32_t pdt_ppn;
    {
        auto &pdt = pdtp[0].get_subtable();
        pdt_ppn = pdt[0].page_ppn;
    }
    {
        auto &pdt = pdtp[1].get_subtable();
        pdt[0].page_ppn = pdt_ppn;
        pdt[0].present = 1;
        pdt[0].os_virt_avail = 1;
    }
    {
        auto &pdt = pdtp[0].get_subtable();
        for (int i = 0; i < 512; i++) {
            pdt[i].present = 0;
        }
    }
    set_pagetable_virt_offset(KERNEL_MEMORY_OFFSET);
}

uint64_t vpagealloc(uint64_t size) {
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;

    pagetable &pml4t = _get_pml4t_cpu0();
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    int i = PMLT4_USERSPACE_START;
    while (i > 0) {
        --i;
        if (pml4t[i].present) {
            auto &pdpt = pml4t[i].get_subtable();
            int j = 512;
            int lj = i != 0 ? 0 : 1;
            while (j > lj) {
                --j;
                if (pdpt[j].present) {
                    auto &pdt = pdpt[j].get_subtable();
                    int k = 512;
                    int lk = (i != 0 || j != 1) ? 0 : 1;
                    while (k > lk) {
                        --k;
                        if (pdt[k].present) {
                            auto &pt = pdt[k].get_subtable();
                            int l = 512;
                            while (l > 0) {
                                --l;
                                /* the page acquire check */
                                if (pt[l].os_virt_avail) {
                                    count++;
                                    if (count == size) {
                                        starting_addr = i << 9;
                                        starting_addr |= j;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= k;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= l;
                                        starting_addr = starting_addr << 12;
                                        uint64_t ending_addr = starting_addr + (count * 4096);
                                        for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                            pageentr *pe = get_pageentr64(pml4t, addr);
                                            pe->os_virt_avail = 0; // GRAB
                                            if (addr == starting_addr) {
                                                pe->os_virt_start = 1; // GRAB
                                            }
                                            pe->present = 0;
                                        }
                                        allocated_vpages += size;
                                        return starting_addr;
                                    }
                                } else {
                                    count = 0;
                                }
                            }
                        } else {
                            count = 0;
                        }
                    }
                } else {
                    count = 0;
                }
            }
        } else {
            count = 0;
        }
    }
    return 0;
}

uint64_t vpagealloc32(uint64_t size) {
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;

    pagetable &pml4t = _get_pml4t_cpu0();
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    int i = 0;
    if (pml4t[i].present) {
        auto &pdpt = pml4t[i].get_subtable();
        int j = (int) (((uint32_t) 0xFFFFFFFF) >> (9+9+12)) + 1;
        int lj = i != 0 ? 0 : 1;
        while (j > lj) {
            --j;
            if (pdpt[j].present) {
                auto &pdt = pdpt[j].get_subtable();
                int k = 512;
                int lk = (i != 0 || j != 1) ? 0 : 1;
                while (k > lk) {
                    --k;
                    if (pdt[k].present) {
                        auto &pt = pdt[k].get_subtable();
                        int l = 512;
                        while (l > 0) {
                            --l;
                            /* the page acquire check */
                            if (pt[l].os_virt_avail) {
                                count++;
                                if (count == size) {
                                    starting_addr = i << 9;
                                    starting_addr |= j;
                                    starting_addr = starting_addr << 9;
                                    starting_addr |= k;
                                    starting_addr = starting_addr << 9;
                                    starting_addr |= l;
                                    starting_addr = starting_addr << 12;
                                    uint64_t ending_addr = starting_addr + (count * 4096);
                                    for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                        pageentr *pe = get_pageentr64(pml4t, addr);
                                        pe->os_virt_avail = 0; // GRAB
                                        if (addr == starting_addr) {
                                            pe->os_virt_start = 1; // GRAB
                                        }
                                        pe->present = 0;
                                    }
                                    allocated_vpages += size;
                                    return starting_addr;
                                }
                            } else {
                                count = 0;
                            }
                        }
                    } else {
                        count = 0;
                    }
                }
            } else {
                count = 0;
            }
        }
    } else {
        count = 0;
    }
    return 0;
}

static VPerCpuPagetables vpercpuallocpagetable_setup(int i, int j, int k) {
    std::shared_ptr<vmem> vm3{};
    if (is_v_multicpu && v_num_cpus > 1) {
        typedef typeof per_cpu_pagetables[0][0] pt_typ;
        auto &cpu0pmlt4 = *((pt_typ *) (((uint8_t *) per_cpu_pagetables[0]) + get_pagetable_virt_offset()));
        auto &cpu1pmlt4 = *((pt_typ *) (((uint8_t *) per_cpu_pagetables[1]) + get_pagetable_virt_offset()));
        vmem vm1{sizeof(pagetable) * v_num_cpus};
        pagetable *tables1 = (pagetable *) vm1.pointer();
        if (cpu0pmlt4[i].page_ppn == cpu1pmlt4[i].page_ppn) {
            auto phys = ppagealloc(sizeof(pagetable) * v_num_cpus);
            vm1.page(0).rwmap(((phys_t) cpu0pmlt4[i].page_ppn) << 12);
            for (int cpu = 1; cpu < v_num_cpus; cpu++) {
                vm1.page(cpu).rwmap(phys + (sizeof(pagetable) * (cpu - 1)));
            }
            vm1.reload();
            for (int cpu = 1; cpu < v_num_cpus; cpu++) {
                memcpy(tables1[cpu], tables1[0], sizeof(tables1[cpu]));
                static_assert(sizeof(tables1[cpu]) == sizeof(pagetable));
                auto &pmlt4 = *((pt_typ *) (((uint8_t *) per_cpu_pagetables[cpu]) + get_pagetable_virt_offset()));
                pmlt4[i].page_ppn = (phys + (sizeof(pagetable) * (cpu - 1))) >> 12;
            }
            reload_pagetables();
        } else {
            for (int cpu = 0; cpu < v_num_cpus; cpu++) {
                auto &pmlt4 = *((pt_typ *) (((uint8_t *) per_cpu_pagetables[cpu]) + get_pagetable_virt_offset()));
                vm1.page(cpu).rwmap(((phys_t) pmlt4[i].page_ppn) << 12);
            }
            reload_pagetables();
        }
        vmem vm2{sizeof(pagetable) * v_num_cpus};
        pagetable *tables2 = (pagetable *) vm2.pointer();
        if (tables1[0][j].page_ppn == tables1[1][j].page_ppn) {
            auto phys = ppagealloc(sizeof(pagetable) * v_num_cpus);
            vm2.page(0).rwmap(((phys_t) tables1[0][j].page_ppn) << 12);
            for (int cpu = 1; cpu < v_num_cpus; cpu++) {
                vm2.page(cpu).rwmap(phys + (sizeof(pagetable) * (cpu - 1)));
            }
            vm2.reload();
            for (int cpu = 1; cpu < v_num_cpus; cpu++) {
                memcpy(tables2[cpu], tables2[0], sizeof(tables2[cpu]));
                static_assert(sizeof(tables2[cpu]) == sizeof(pagetable));
                auto &tbl = tables1[cpu];
                tbl[j].page_ppn = (phys + (sizeof(pagetable) * (cpu - 1))) >> 12;
            }
            reload_pagetables();
        } else {
            for (int cpu = 0; cpu < v_num_cpus; cpu++) {
                auto &tbl = tables1[cpu];
                vm2.page(cpu).rwmap(((phys_t) tbl[j].page_ppn) << 12);
            }
            reload_pagetables();
        }
        vm3 = std::make_shared<vmem>(sizeof(pagetable) * v_num_cpus);
        pagetable *tables3 = (pagetable *) vm3->pointer();
        if (tables2[0][k].page_ppn == tables2[1][k].page_ppn) {
            auto phys = ppagealloc(sizeof(pagetable) * v_num_cpus);
            vm3->page(0).rwmap(((phys_t) tables2[0][k].page_ppn) << 12);
            for (int cpu = 1; cpu < v_num_cpus; cpu++) {
                vm3->page(cpu).rwmap(phys + (sizeof(pagetable) * (cpu - 1)));
            }
            vm3->reload();
            for (int cpu = 1; cpu < v_num_cpus; cpu++) {
                memcpy(tables2[cpu], tables2[0], sizeof(tables2[cpu]));
                static_assert(sizeof(tables2[cpu]) == sizeof(pagetable));
                auto &tbl = tables2[cpu];
                tbl[k].page_ppn = (phys + (sizeof(pagetable) * (cpu - 1))) >> 12;
            }
            reload_pagetables();
        } else {
            for (int cpu = 0; cpu < v_num_cpus; cpu++) {
                auto &tbl = tables2[cpu];
                vm3->page(cpu).rwmap(((phys_t) tbl[k].page_ppn) << 12);
            }
            reload_pagetables();
        }
    }
    uintptr_t ptr = (((uintptr_t) i) << (9+9+9+12)) + (((uintptr_t) j) << (9+9+12)) + (((uintptr_t) k) << (9+12));
    return {.vm = vm3, .tables = (pagetable *) vm3->pointer(), .numTables = v_num_cpus, .pointer = ptr};
}

VPerCpuPagetables vpercpuallocpagetable() {
    std::unique_lock lock{get_pagetables_lock()};

    pagetable &pml4t = _get_pml4t_cpu0();
    int i = PMLT4_USERSPACE_START;
    while (i > 0) {
        --i;
        if (pml4t[i].present) {
            auto &pdpt = pml4t[i].get_subtable();
            int j = 512;
            while (j > 0) {
                --j;
                if (pdpt[j].present) {
                    auto &pdt = pdpt[j].get_subtable();
                    int k = 512;
                    while (k > 0) {
                        --k;
                        if (pdt[k].present) {
                            auto &pt = pdt[k].get_subtable();
                            bool suitable{true};
                            int l = 512;
                            while (l > 0) {
                                --l;
                                /* the page acquire check */
                                if (pt[l].os_virt_avail == 0) {
                                    suitable = false;
                                    break;
                                }
                            }
                            if (suitable) {
                                l = 512;
                                while (l > 0) {
                                    --l;
                                    pt[l].os_virt_avail = 0;
                                }
                                pt[l].os_virt_start = 1;
                                lock.release();
                                return vpercpuallocpagetable_setup(i, j, k);
                            }
                        }
                    }
                }
            }
        }
    }
    return {};
}

VPerCpuPagetables vpercpuallocpagetable32() {
    std::unique_lock lock{get_pagetables_lock()};

    pagetable &pml4t = _get_pml4t_cpu0();
    int i = 0;
    if (pml4t[i].present) {
        auto &pdpt = pml4t[i].get_subtable();
        int j = (int) (((uint32_t) 0xFFFFFFFF) >> (9+9+12)) + 1;
        while (j > 0) {
            --j;
            if (pdpt[j].present) {
                auto &pdt = pdpt[j].get_subtable();
                int k = 512;
                while (k > 0) {
                    --k;
                    if (pdt[k].present) {
                        auto &pt = pdt[k].get_subtable();
                        bool suitable{true};
                        int l = 512;
                        while (l > 0) {
                            --l;
                            /* the page acquire check */
                            if (pt[l].os_virt_avail == 0) {
                                suitable = false;
                                break;
                            }
                        }
                        if (suitable) {
                            l = 512;
                            while (l > 0) {
                                --l;
                                pt[l].os_virt_avail = 0;
                            }
                            pt[l].os_virt_start = 1;
                            lock.release();
                            return vpercpuallocpagetable_setup(i, j, k);
                        }
                    }
                }
            }
        }
    }
    return {};
}

uint32_t vpagetablecount() {
    return 512;
}

void pmemcounts() {
    uint64_t scan = 0;
    uint64_t free_pages = 0;
    {
        std::lock_guard lock{get_pagetables_lock()};

        auto *phys = get_physpagemap();
        for (uint32_t i = 0; i < phys->max(); i++) {
            if (phys->claimed(i)) {
                ++free_pages;
            }
            ++scan;
        }
    }
    get_klogger() << "Ppages " << scan << " pages scanned, " << free_pages << " free encountered\n";
}

//#define DEBUG_PALLOC_FAILURE
static phys_t ppagealloc_locked(uintptr_t size) {
#ifdef DEBUG_PALLOC_FAILURE
    uint64_t scan = 0;
    uint64_t free_pages = 0;
    {
#endif

        if ((size & 4095) != 0) {
            size += 4096;
        }
        size /= 4096;
        uint64_t starting_addr = 0;
        uint64_t count = 0;
        auto *phys = get_physpagemap();
        for (uint32_t page = 512; page < phys->max(); page++) {
            if (!phys->claimed(page)) {
                if (starting_addr != 0) {
                    count++;
                    if (count == size) {
                        uint64_t ending_addr = starting_addr + (count * 4096);
                        for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                            phys->claim(addr >> 12); // GRAB
                        }
                        allocated_ppages += size;
                        return starting_addr;
                    }
                } else {
                    starting_addr = page;
                    starting_addr = starting_addr << 12;
                    if (size == 1) {
                        phys->claim(starting_addr >> 12); // GRAB
                        ++allocated_ppages;
                        return starting_addr;
                    } else {
                        count = 1;
                    }
                }
            } else {
                starting_addr = 0;
            }
        }
#ifdef DEBUG_PALLOC_FAILURE
    }
    get_klogger() << "Ppage allocation failure " << scan << " pages scanned, " << free_pages << " free encountered\n";
#endif
    return 0;
}

phys_t ppagealloc(uintptr_t size) {
    std::lock_guard lock{get_pagetables_lock()};
    return ppagealloc_locked(size);
}

phys_t ppagealloc32(uint32_t size) {
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    uint32_t starting_addr = 0;
    uint32_t count = 0;

    auto *phys = get_physpagemap();
    uint32_t max = (0xFFFFFFFF >> 12) + 1;
    if (max > phys->max()) {
        max = phys->max();
    }
    for (uint32_t page = 256; page < max; page++) {
        if (!phys->claimed(page)) {
            if (starting_addr != 0) {
                count++;
                if (count == size) {
                    uint64_t ending_addr = starting_addr + (count * 4096);
                    for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                        phys->claim(addr >> 12); // GRAB
                    }
                    allocated_ppages += size;
                    return starting_addr;
                }
            } else {
                starting_addr = page;
                starting_addr = starting_addr << 12;
                if (size == 1) {
                    phys->claim(starting_addr >> 12); // GRAB
                    ++allocated_ppages;
                    return starting_addr;
                } else {
                    count = 1;
                }
            }
        } else {
            starting_addr = 0;
        }
    }
    return 0;
}

uint64_t vpagefree(uint64_t addr) {
    std::lock_guard lock{get_pagetables_lock()};

    pagetable &pml4t = _get_pml4t_cpu0();
    addr = addr >> 12;
    uint64_t first = addr;
    uint64_t size{0};
    while (true) {
        uint64_t paddr = addr;
        int l = (int) (paddr & 511);
        paddr = paddr >> 9;
        int k = (int) (paddr & 511);
        paddr = paddr >> 9;
        int j = (int) (paddr & 511);
        paddr = paddr >> 9;
        int i = (int) (paddr & 511);
        if (pml4t[i].present == 0) {
            break;
        }
        auto &pdpt = pml4t[i].get_subtable();
        if (pdpt[j].present == 0) {
            break;
        }
        auto &pdt = pdpt[j].get_subtable();
        if (pdt[k].present == 0) {
            break;
        }
        pageentr &pe = pdt[k].get_subtable()[l];
        if (pe.os_virt_avail || (first != addr && pe.os_virt_start)) {
            break;
        }
        pe.os_virt_avail = 1;
        pe.os_virt_start = 0;
        pe.present = 0;
        ++addr;
        ++size;
    }
    allocated_vpages -= size;
    return size << 12;
}

uint64_t vpagesize(uint64_t addr) {
    std::lock_guard lock{get_pagetables_lock()};

    const pagetable &pml4t = _get_pml4t_cpu0();
    addr = addr >> 12;
    uint64_t first = addr;
    uint64_t size{0};
    while (true) {
        uint64_t paddr = addr;
        int l = (int) (paddr & 511);
        paddr = paddr >> 9;
        int k = (int) (paddr & 511);
        paddr = paddr >> 9;
        int j = (int) (paddr & 511);
        paddr = paddr >> 9;
        int i = (int) (paddr & 511);
        if (pml4t[i].present == 0) {
            break;
        }
        const auto &pdpt = pml4t[i].get_subtable();
        if (pdpt[j].present == 0) {
            break;
        }
        const auto &pdt = pdpt[j].get_subtable();
        if (pdt[k].present == 0) {
            break;
        }
        const pageentr &pe = pdt[k].get_subtable()[l];
        if (pe.os_virt_avail || (first != addr && pe.os_virt_start)) {
            break;
        }
        ++addr;
        ++size;
    }
    return size << 12;
}


uint64_t pv_fixp1g_pagealloc(uint64_t size) {
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = _get_pml4t_cpu0();
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    auto *phys = get_physpagemap();
    for (int i = 0; i < PMLT4_USERSPACE_START; i++) {
        if (pml4t[i].present) {
            auto &pdpt = pml4t[i].get_subtable();
            for (int j = i != 0 ? 0 : 1; j < 512; j++) {
                if (pdpt[j].present) {
                    auto &pdt = pdpt[j].get_subtable();
                    for (int k = i == 0 && j == 1 ? 1 : 0; k < 512; k++) {
                        if (pdt[k].present) {
                            auto &pt = pdt[k].get_subtable();
                            for (int l = 0; l < 512; l++) {
                                /* the page acquire check */
                                uint64_t p_addr = i;
                                p_addr = p_addr << 9;
                                p_addr += j;
                                p_addr = p_addr << 9;
                                p_addr += k;
                                p_addr = p_addr << 9;
                                p_addr += l;
                                p_addr -= KERNEL_MEMORY_OFFSET >> 12;
                                if (p_addr < phys->max() && !phys->claimed(p_addr) && pt[l].os_virt_avail == 1) {
                                    p_addr = p_addr << 12;
                                    if (starting_addr != 0) {
                                        count++;
                                        if (count == size) {
                                            uint64_t ending_addr = starting_addr + (count * 4096);
                                            for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                                pageentr *pe = get_pageentr64(pml4t, addr + KERNEL_MEMORY_OFFSET);
                                                pe->os_virt_avail = 0; // GRAB
                                                phys->claim(addr >> 12); // GRAB
                                                if (addr == starting_addr) {
                                                    pe->os_virt_start = 1;
                                                }
                                                pe->page_ppn = addr >> 12;
                                                pe->present = 1;
                                                pe->writeable = 1;
                                                pe->execution_disabled = 1;
                                                pe->accessed = 0;
                                                pe->user_access = 0;
                                            }
                                            reload_pagetables();
                                            allocated_ppages += size;
                                            allocated_vpages += size;
                                            return starting_addr;
                                        }
                                    } else {
                                        starting_addr = p_addr;
                                        if (size == 1) {
                                            pageentr &pe = pt[l];
                                            pe.os_virt_avail = 0; // GRAB
                                            phys->claim(starting_addr >> 12); // GRAB
                                            pe.os_virt_start = 1; // starting point
                                            pe.page_ppn = starting_addr >> 12;
                                            pe.present = 1;
                                            pe.writeable = 1;
                                            pe.execution_disabled = 1;
                                            pe.accessed = 0;
                                            pe.user_access = 0;
                                            ++allocated_ppages;
                                            ++allocated_vpages;
                                            return starting_addr;
                                        } else {
                                            count = 1;
                                        }
                                    }
                                } else {
                                    starting_addr = 0;
                                }
                            }
                        } else {
                            starting_addr = 0;
                        }
                    }
                } else {
                    starting_addr = 0;
                }
            }
        } else {
            starting_addr = 0;
        }
    }
    return 0;
}

uint64_t pv_fixp1g_pagefree(uint64_t vaddr) {
    uint64_t vai = vaddr;
    vai += KERNEL_MEMORY_OFFSET;
    uint64_t phys = get_phys_from_virt(vai);
    uint64_t size = vpagefree(vaddr);
    ppagefree(phys, size);
    reload_pagetables();
    return size;
}

uint64_t alloc_stack(uint64_t size) {
    uint64_t vaddr_top = 0;
    uint64_t vaddr_bottom = vpagealloc(size + 4096);
    if (vaddr_bottom != 0) {
        uint64_t vaddr_start = vaddr_bottom + 4096;
        uint64_t paddr_start = ppagealloc(size);
        if (paddr_start != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                std::optional<pageentr> pe = get_pageentr(vaddr_start + offset);
                uint64_t page_ppn = paddr_start + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                pe->execution_disabled = 1;
                update_pageentr(vaddr_start + offset, *pe);
            }
            vaddr_top = vaddr_start + size;

            reload_pagetables();
        } else {
            vpagefree(vaddr_bottom);
            vaddr_top = 0;
        }
    }
    return vaddr_top;
}

void free_stack(uint64_t vaddr) {
    uint64_t size = 0;
    uint64_t phys_addr = 0;
    vaddr = vaddr >> 12;
    {
        std::lock_guard lock{get_pagetables_lock()};

        while (true) {
            --vaddr;
            uint64_t paddr = vaddr;
            int l = paddr & 511;
            paddr = paddr >> 9;
            int k = paddr & 511;
            paddr = paddr >> 9;
            int j = paddr & 511;
            paddr = paddr >> 9;
            int i = paddr & 511;
            if (_get_pml4t_cpu0()[i].present == 0) {
                wild_panic("Stack is outside allocatable");
            }
            auto &pdpt = _get_pml4t_cpu0()[i].get_subtable();
            if (pdpt[j].present == 0) {
                wild_panic("Stack is outside allocatable");
            }
            auto &pdt = pdpt[j].get_subtable();
            if (pdt[k].present == 0) {
                wild_panic("Stack is outside allocatable");
            }
            pageentr &pe = pdt[k].get_subtable()[l];
            if (pe.os_virt_avail == 1) {
                get_klogger() << "\nPage " << vaddr << " not allocated\n";
                wild_panic("Stack address given runs into unallocated vpage");
            }
            bool first_page = pe.os_virt_start == 1;
            if (pe.present) {
                if (first_page) {
                    wild_panic("This was not a stack allocation");
                }
                phys_addr = pe.page_ppn;
                ++size;
            } else {
                if (!first_page) {
                    wild_panic("Part of stack is not present");
                }
            }

            pe.os_virt_avail = 1;
            pe.os_virt_start = 0;

            --allocated_vpages;

            if (first_page) {
                break;
            }

            pe.present = 0;
        }
    }
    phys_addr = phys_addr << 12;
    size = size << 12;

    ppagefree(phys_addr, size);
    reload_pagetables();
}

void reload_pagetables() {
    critical_section cli{};
    uint64_t cr3 = (uint64_t) &(_get_pml4t_this_cpu()) - get_pagetable_virt_offset();
    asm("mov %0,%%cr3; " :: "r"(cr3));
}

void ppagefree(uint64_t addr, uint64_t size) {
    std::lock_guard lock{get_pagetables_lock()};

    pagetable &pml4t = _get_pml4t_cpu0();
    addr = addr >> 12;
    if ((size & 4095) != 0) {
        size += 4096;
    }
    size = size >> 12;
    uint64_t first = addr;
    auto *phys = get_physpagemap();
    while (size > 0) {
        uint64_t paddr = addr;
        int l = (int) (paddr & 511);
        paddr = paddr >> 9;
        int k = (int) (paddr & 511);
        paddr = paddr >> 9;
        int j = (int) (paddr & 511);
        paddr = paddr >> 9;
        int i = (int) (paddr & 511);
        if (!phys->claimed(addr) || addr >= phys->max()) {
            wild_panic("PFree pointed at available page");
        }
        phys->release(addr);
        ++addr;
        --size;
        --allocated_ppages;
    }
}

void *pagealloc_phys32(uint64_t size) {
    uint64_t vpages = vpagealloc(size);
    if (vpages != 0) {
        uint64_t ppages = ppagealloc32(size);
        if (ppages != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                std::optional<pageentr> pe = get_pageentr(vpages + offset);
                uint64_t page_ppn = ppages + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->execution_disabled = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                update_pageentr(vpages + offset, *pe);
            }

            reload_pagetables();
        } else {
            vpagefree(vpages);
            vpages = 0;

            reload_pagetables();
        }
    }
    return (void *) vpages;
}

void *pagealloc32(uint64_t size) {
    uint64_t vpages = vpagealloc32(size);
    if (vpages != 0) {
        uint64_t ppages = ppagealloc32(size);
        if (ppages != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                std::optional<pageentr> pe = get_pageentr(vpages + offset);
                uint64_t page_ppn = ppages + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->execution_disabled = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                update_pageentr(vpages + offset, *pe);
            }

            reload_pagetables();
        } else {
            vpagefree(vpages);
            vpages = 0;

            reload_pagetables();
        }
    }
    return (void *) vpages;
}

void *pagealloc(uint64_t size) {
    uint64_t vpages = vpagealloc(size);
    if (vpages != 0) {
        uint64_t ppages = ppagealloc(size);
        if (ppages != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                std::optional<pageentr> pe = get_pageentr(vpages + offset);
                uint64_t page_ppn = ppages + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->execution_disabled = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                update_pageentr(vpages + offset, *pe);
            }

            reload_pagetables();
        } else {
            vpagefree(vpages);
            vpages = 0;

            reload_pagetables();
        }
    }
    return (void *) vpages;
}

void pagefree(void *vaddr) {
    uint64_t vai = (uint64_t) vaddr;
    uint64_t phys = get_phys_from_virt(vai);
    uint64_t size = vpagefree(vai);
    ppagefree(phys, size);
    reload_pagetables();
}

class pvpage_stats : public statistics_object {
public:
    void Accept(statistics_visitor &visitor) override {
        long long int total_p, total_v, allocated_p, allocated_v;
        {
            std::lock_guard lock{get_pagetables_lock()};
            total_p = total_ppages;
            total_v = total_vpages;
            allocated_p = allocated_ppages;
            allocated_v = allocated_vpages;
        }
        visitor.Visit("total_ppages", total_p);
        visitor.Visit("kernel_vpages", total_v);
        visitor.Visit("allocated_p", allocated_p);
        visitor.Visit("allocated_v", allocated_v);
    }
};

class pvpages_stats_factory : public statistics_object_factory {
public:
    std::shared_ptr<statistics_object> GetObject() override {
        return std::make_shared<pvpage_stats>();
    }
};

void vmem_switch_to_multicpu(ApStartup *apStartupP, int numCpus) {
    critical_section cli{};
    apStartup = apStartupP;
    per_cpu_pagetables = (pagetable **) (void *) pagealloc(numCpus * sizeof(*per_cpu_pagetables));
    v_num_cpus = numCpus;
    for (int i = 0; i < numCpus; i++) {
        per_cpu_pagetables[i] = (pagetable *) (void *) pv_fixp1g_pagealloc(sizeof(*(per_cpu_pagetables[i])));
        memcpy(((uint8_t *) (per_cpu_pagetables[i])) + KERNEL_MEMORY_OFFSET, &(_get_pml4t_this_cpu()), sizeof(*(per_cpu_pagetables[i])));
        static_assert(sizeof(*(per_cpu_pagetables[i])) == 4096);
    }
    vmem_set_per_cpu_pagetables();
    is_v_multicpu = true;
}

void vmem_set_per_cpu_pagetables() {
    pagetable *table = per_cpu_pagetables[apStartup->GetCpuNum()];
    std::cout << "Set per-cpu pagetables root to " << std::hex << ((uintptr_t) table) << std::dec << "\n";
    asm("mov %0, %%rax; mov %%rax, %%cr3" :: "r"(table) : "%rax");
}

void setup_pvpage_stats() {
    {
        long long int visited_nonavail_phys = 0;
        std::lock_guard lock{get_pagetables_lock()};
        total_ppages = 0;
        total_vpages = 0;
        allocated_ppages = 0;
        allocated_vpages = 0;
        auto *phys = get_physpagemap();
        pagetable &pml4t = _get_pml4t_cpu0();
        for (int i = 0; i < 512; i++) {
            if (pml4t[i].present) {
                auto &pdpt = pml4t[i].get_subtable();
                for (int j = 0; j < 512; j++) {
                    if (pdpt[j].present) {
                        auto &pdt = pdpt[j].get_subtable();
                        for (int k = 0; k < 512; k++) {
                            if (pdt[k].present) {
                                auto &pt = pdt[k].get_subtable();
                                for (int l = 0; l < 512; l++) {
                                    ++total_vpages;
                                    uint64_t paddr = i;
                                    paddr = paddr << 9;
                                    paddr += j;
                                    paddr = paddr << 9;
                                    paddr += k;
                                    paddr = paddr << 9;
                                    paddr += l;
                                    if (paddr < phys->max() && !phys->claimed(paddr)) {
                                        allocated_ppages += visited_nonavail_phys;
                                        visited_nonavail_phys = 0;
                                        if (total_vpages > total_ppages) {
                                            total_ppages = total_vpages;
                                        }
                                    } else {
                                        ++visited_nonavail_phys;
                                    }
                                    if (pt[l].os_virt_avail == 0) {
                                        ++allocated_vpages;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    GetStatisticsRoot().Add("pagealloc", std::make_shared<pvpages_stats_factory>());
}
