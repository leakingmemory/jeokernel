//
// Created by sigsegv on 25.04.2021.
//

#ifndef JEOKERNEL_GLOBALDESCRIPTORTABLE_H
#define JEOKERNEL_GLOBALDESCRIPTORTABLE_H

#include <loaderconfig.h>
#include <pagetable.h>

#define TSS_GD(cpu) (cpu + 4)
#define TSS_MAX_CPUS (GDT_SIZE - 4)

class GlobalDescriptorTable {
private:
    GDT_table<GDT_SIZE> *gdt;
public:
    GlobalDescriptorTable()  : gdt((typeof(gdt)) GDT_ADDR) {
    }

    uint16_t get_selector(int n) const {
        return sizeof(gdt->gdt[0]) * n;
    }
    int get_index_from_selector(int selector) const {
        return selector / sizeof(gdt->gdt[0]);
    }
    GDT &get_descriptor(int n) {
        return gdt->gdt[n];
    }
    uint16_t get_number_of_descriptors() const {
        return GDT_SIZE;
    }
    uint16_t get_max_tss_cpus() const {
        return TSS_MAX_CPUS;
    }

    void reload() {
        uint64_t gdt64 = GDT_ADDR;
        asm("lgdt (%0)" :: "r"(gdt64));
    }
};


#endif //JEOKERNEL_GLOBALDESCRIPTORTABLE_H
