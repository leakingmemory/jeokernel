//
// Created by sigsegv on 25.04.2021.
//

#ifndef JEOKERNEL_GLOBALDESCRIPTORTABLE_H
#define JEOKERNEL_GLOBALDESCRIPTORTABLE_H

#include <loaderconfig.h>
#include <pagetable.h>

#define BSP_TSS_GD  6
#define TSS_GD(cpu) (((cpu) * 2) + 8)
#define TSS_MAX_CPUS ((GDT_SIZE - 8) / 2)

class GlobalDescriptorTable {
private:
    GDT_table<GDT_SIZE + (LDT_DESC_SIZE * 2)> *gdt;
public:
    GlobalDescriptorTable()  : gdt((typeof(gdt)) GDT_ADDR) {
    }

    constexpr uint16_t get_selector(int n) const {
        return sizeof(gdt->gdt[0]) * n;
    }
    constexpr uint16_t get_ldt_selector(int n) const {
        return sizeof(gdt->gdt[0]) * (GDT_SIZE + (n * 2));
    }
    int get_index_from_selector(int selector) const {
        return selector / sizeof(gdt->gdt[0]);
    }
    GDT &get_descriptor(int n) {
        return gdt->gdt[n];
    }
    LDTP &get_ldt_descriptor(int n) {
        return *((LDTP *) (void *) &(gdt->gdt[GDT_SIZE + (n * 2)]));
    }
    constexpr uint16_t get_number_of_descriptors() const {
        return GDT_SIZE;
    }
    constexpr uint16_t get_max_tss_cpus() const {
        return TSS_MAX_CPUS;
    }

    void reload() {
        uint64_t gdt64 = GDT_ADDR + ((GDT_SIZE + (LDT_DESC_SIZE * 2)) * 8);
        asm("lgdt (%0)" :: "r"(gdt64));
    }
};


#endif //JEOKERNEL_GLOBALDESCRIPTORTABLE_H
