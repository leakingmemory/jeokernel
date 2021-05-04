//
// Created by sigsegv on 01.05.2021.
//

#ifndef JEOKERNEL_CPU_MPFP_H
#define JEOKERNEL_CPU_MPFP_H

#include <tuple>
#include <vector>

struct mp_floating_pointer {
    uint32_t signature;
    uint32_t phys_pointer;
    uint8_t length;
    uint8_t spec_rev;
    uint8_t checksum;
    uint64_t feature : 40;

    bool valid();
} __attribute__((__packed__));
static_assert(sizeof(mp_floating_pointer) == 0x10);

struct mp_cpu_entry {
    uint8_t entry_type;// = 0
    uint8_t local_apic_id;
    uint8_t local_apic_version;
    uint8_t cpu_flags;
    uint32_t cpu_signature;
    uint32_t features;
    uint32_t reserved1;
    uint32_t reserved2;
} __attribute__((__packed__));
static_assert(sizeof(mp_cpu_entry) == 20);

struct mp_bus_entry {
    uint8_t entry_type;// = 1
    uint8_t bus_id;
    int8_t bus_type[6];
} __attribute__((__packed__));
static_assert(sizeof(mp_bus_entry) == 8);

struct mp_ioapic_entry {
    uint8_t entry_type;// = 2
    uint8_t ioapic_id;
    uint8_t ioapic_version;
    uint8_t ioapic_flags;
    uint32_t mapped_memory_addr;
} __attribute__((__packed__));

struct mp_ioapic_interrupt_entry {
    uint8_t entry_type; // = 3
    uint8_t interrupt_type;
    uint16_t interrupt_flags;
    uint8_t source_bus_id;
    uint8_t source_bus_irq;
    uint8_t destination_ioapic_id;
    uint8_t ioapic_intin;
} __attribute__((__packed__));
static_assert(sizeof(mp_ioapic_interrupt_entry) == 8);

struct mp_local_interrupt_entry {
    uint8_t entry_type; // = 4
    uint8_t interrupt_type;
    uint16_t interrupt_flags;
    uint8_t source_bus_id;
    uint8_t source_bus_irq;
    uint8_t destination_apic_id;
    uint8_t apic_lintin;
} __attribute__((__packed__));
static_assert(sizeof(mp_local_interrupt_entry) == 8);

struct mp_any_entry {
    uint8_t entry_type;
    uint8_t entry_length;

    const mp_cpu_entry &cpu() const {
        return *((mp_cpu_entry *) this);
    }
    const mp_bus_entry &bus() const {
        return *((mp_bus_entry *) this);
    }
    const mp_ioapic_entry &ioapic() const {
        return *((mp_ioapic_entry *) this);
    }
    const mp_ioapic_interrupt_entry &ioapic_int() const {
        return *((mp_ioapic_interrupt_entry *) this);
    }
    const mp_local_interrupt_entry &local_int() const {
        return *((mp_local_interrupt_entry *) this);
    }

    const mp_any_entry *next() const {
        const uint8_t *ptr = &entry_type;
        switch (entry_type) {
            case 0: // cpu
                ptr += 20;
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                ptr += 8;
                break;
            default:
                if (entry_length > 2) {
                    ptr += entry_length;
                } else {
                    ptr += 8;
                }
        }
        return (mp_any_entry *) (void *) ptr;
    }
} __attribute__((__packed__));

struct mp_configuration_table_header {
    uint32_t signature;
    uint16_t tbl_length;
    uint8_t spec_rev;
    uint8_t checksum;
    int8_t oemid[4];
    int8_t prodid[16];
    uint32_t oem_table_pointer;
    uint16_t oem_table_size;
    uint16_t table_entries;
    uint32_t local_apic_map_addr;
    uint16_t extended_table_length;
    uint8_t extended_table_checksum;
    uint8_t reserved;

    bool valid();

    const mp_any_entry *first_entry() const {
        const uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += sizeof(*this);
        return (const mp_any_entry *) (void *) ptr;
    }
} __attribute__((__packed__));
static_assert(sizeof(mp_configuration_table_header) == 0x2C);

#define MAX_CPUs 32
#define MAX_BUSs 32
#define MAX_IOAPICs 2
#define MAX_INTs 64
#define MAX_LOCAL_INTs (MAX_CPUs * 16)

class cpu_mpfp {
private:
    mp_floating_pointer mpfp;
    mp_configuration_table_header mp_tbl_hdr;
    mp_cpu_entry cpus[MAX_CPUs];
    mp_bus_entry bus[MAX_BUSs];
    mp_ioapic_entry ioapic[MAX_IOAPICs];
    mp_ioapic_interrupt_entry ioapic_ints[MAX_INTs];
    mp_local_interrupt_entry local_ints[MAX_LOCAL_INTs];
    uint8_t ncpu;
    uint8_t nbus;
    uint8_t nioapic;
    uint16_t n_ioapic_int;
    uint16_t n_local_int;
public:
    /**
     *
     * @param res_mem Reserved memory ranges <start,size>
     */
    explicit cpu_mpfp(const std::vector<std::tuple<uint64_t,uint64_t>> &res_mem);

    int get_num_cpus() const {
        return ncpu;
    }
    const mp_cpu_entry &get_cpu(int n) const {
        return cpus[n];
    }
    int get_current_cpu_num() const;

    uint32_t get_local_apic_addr() const {
        return mp_tbl_hdr.local_apic_map_addr;
    }
    int get_num_ioapics() const {
        return nioapic;
    }
    const mp_ioapic_entry &get_ioapic(int idx) const {
        return ioapic[idx];
    }
};


#endif //JEOKERNEL_CPU_MPFP_H
