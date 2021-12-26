//
// Created by sigsegv on 17.04.2021.
//

#ifndef JEOKERNEL_PAGETABLE_H
#define JEOKERNEL_PAGETABLE_H

#include <stdint.h>

struct GDT {
    uint16_t limit_low : 16;
    uint32_t base_low : 24;
    uint8_t type : 8;
    uint8_t limit_high : 4;
    uint8_t granularity : 4;
    uint8_t base_high : 8;

    GDT() {
        limit_low = 0xFFFF;
        base_low = 0;
        type = 0;
        limit_high = 1;
        granularity = 0;
        base_high = 0;
    }
    GDT(uint32_t base, uint32_t limit, uint8_t granularity, uint8_t type) :
        limit_low(limit & 0xFFFF),
        base_low(base & 0xFFFFFF),
        type(type),
        limit_high((limit >> 16) & 0x0F),
        granularity(granularity & 0x0F),
        base_high(base >> 24)
    {
    }

    void set_base(uint32_t base_addr) {
        base_low = base_addr & 0xFFFFFF;
        base_high = (base_addr >> 24);
    }
    void set_limit(uint32_t limit) {
        limit_low = limit & 0xFFFF;
        limit_high = (limit >> 16) & 0x0F;
    }
} __attribute__((__packed__));

struct GDT_addr_64_in_32 {
    uint32_t low;
    uint32_t high;
};

union GDT_addr {
    GDT_addr_64_in_32 addr64in32;
    uint64_t addr;
};
static_assert(sizeof(GDT_addr) == 8);

template <int n> struct GDT_table {
    static_assert(n > 0);
    GDT gdt[n];
    uint16_t size;
    GDT_addr ptr;

    GDT_table() {
        for (int i = 0; i < n; i++) {
            gdt[i] = GDT();
        }
    }

    void set_size() {
        size = sizeof(gdt) - 1;
    }
    void set_ptr() {
        ptr.addr = (uint64_t) this;
    }

    GDT & operator [] (int i) {
        if (i < 0) {
            return gdt[0];
        }
        if (i < n) {
            return gdt[i];
        } else {
            return gdt[n -1];
        }
    }

    uint64_t pointer() {
        set_size();
        set_ptr();
        return (uint64_t) &size;
    }
} __attribute__((__packed__));

static_assert(sizeof(GDT) == 8);

struct pageentr {
    uint8_t present : 1;
    uint8_t writeable : 1;
    uint8_t user_access : 1;
    uint8_t write_through : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed : 1;
    uint8_t dirty : 1;
    uint8_t size : 1;
    uint8_t global : 1;
    uint8_t os_phys_avail : 1;
    uint8_t os_zero : 1;
    uint8_t os_virt_avail : 1;
    uint32_t page_ppn : 28;
    uint16_t reserved1 : 12;
    uint8_t os_virt_start : 1;
    uint16_t ignored3 : 10;
    uint8_t execution_disabled : 1;

    uint64_t get_subtable_addr() const {
        uint64_t addr = page_ppn;
        addr *= 4096;
        return addr;
    }
    typedef pageentr pdpt[512];
    pdpt &get_subtable() const {
        return *((pdpt *) get_subtable_addr());
    }
} __attribute__((__packed__));

static_assert(sizeof(pageentr) == 8);

typedef pageentr pagetable[512];

pageentr &get_pml4t_pageentr64(pagetable &pml4t, uint64_t addr);
pageentr &get_pdpt_pageentr64(pagetable &pdpt_ref, uint64_t addr);
pageentr &get_pdt_pageentr64(pagetable &pdt_ref, uint64_t addr);
pageentr &get_pt_pageentr64(pagetable &pt_ref, uint64_t addr);
pageentr *get_pageentr64(pagetable &pml4t, uint64_t addr);

#ifndef LOADER

#include <optional>
#include <functional>

#ifndef UNIT_TESTING
#include <concurrency/hw_spinlock.h>

#endif

void initialize_pagetable_control();

hw_spinlock &get_pagetables_lock();

uint64_t get_phys_from_virt(uint64_t vaddr);
std::optional<pageentr> get_pageentr(uint64_t addr);
/**
 * Update the vmem properties of the pageentr. Any allocation
 * bits, reserved bits, and similar will be left unchanged.
 *
 * @param addr
 * @param pe_vmem_update
 * @return True on success.
 */
bool update_pageentr(uint64_t addr, const pageentr &pe_vmem_update);
bool update_pageentr(uint64_t addr, std::function<void (pageentr &pe)>);

#endif
#endif //JEOKERNEL_PAGETABLE_H
