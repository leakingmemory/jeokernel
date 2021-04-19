//
// Created by sigsegv on 17.04.2021.
//

#ifndef JEOKERNEL_PAGETABLE_H
#define JEOKERNEL_PAGETABLE_H

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
        size = sizeof(gdt) - 1;
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
        ptr.addr = (uint64_t) this;
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
    uint8_t ignored1 : 1;
    uint8_t size : 1;
    uint8_t ignored2 : 4;
    uint32_t page_ppn : 28;
    uint16_t reserved1 : 12;
    uint16_t ignored3 : 11;
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

#endif //JEOKERNEL_PAGETABLE_H
