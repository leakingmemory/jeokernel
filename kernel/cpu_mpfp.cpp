//
// Created by sigsegv on 01.05.2021.
//

#include <klogger.h>
#include <core/cpu_mpfp.h>
#include <core/vmem.h>
#include <cpuid.h>

static mp_floating_pointer *search_page(void *ptr, size_t size) {
    uint32_t *p = (uint32_t *) ptr;
    uint32_t *e = p + (size >> 2);
    while (p < e) {
        if (*p == 0x5F504D5F) /* _PM_ */ {
            mp_floating_pointer *mpfp = (mp_floating_pointer *) (void *) p;
            if (mpfp->valid()) {
                return mpfp;
            }
        }
        p += 4;
    }
    return nullptr;
}

cpu_mpfp::cpu_mpfp(const std::vector<std::tuple<uint64_t, uint64_t>> &res_mem) :
ncpu(0), nbus(0), nioapic(0), n_ioapic_int(0), n_local_int(0),
cpus(), bus(), ioapic(), ioapic_ints(), local_ints(), valid(false) {
    vmem vm{0x2000};
    mp_configuration_table_header *mpc;

    for (const auto &res : res_mem) {
        uint64_t start_addr = std::get<0>(res);
        uint64_t size = std::get<1>(res);
        for (uint64_t offset = 0; offset < size; offset += 4096) {
            uint64_t s = size - offset;
            bool npage = false;
            if (s > 0x1000) {
                s = 0x1000;
                npage = true;
            }
            uint64_t addr = start_addr + offset;
            vm.page(0).rmap(addr);
            if (npage) {
                vm.page(1).rmap(addr + 0x1000);
            } else {
                vm.page(1).unmap();
            }
            vm.reload();
            mp_floating_pointer *result = search_page(vm.pointer(), s);
            if (result != nullptr) {
                mpfp = *result;
                goto locate_mpc;
            }
        }
    }
locate_mpc:
    if (mpfp.phys_pointer != 0) {
        mpc = nullptr;
        for (const auto &res : res_mem) {
            uint64_t baseaddr = std::get<0>(res);
            uint64_t size = std::get<1>(res);
            if (mpfp.phys_pointer >= baseaddr && mpfp.phys_pointer < (baseaddr + size)) {
                uint64_t pageaddr = mpfp.phys_pointer & 0xFFFFF000;
                vm.page(0).rmap(pageaddr);
                uint64_t maxsize = (baseaddr + size) - pageaddr;
                if (maxsize >= 0x2000) {
                    vm.page(1).rmap(pageaddr + 0x1000);
                }
                vm.reload();
                mpc = (mp_configuration_table_header *) (void *) ((uint64_t) mpfp.phys_pointer);
                goto mpc_pointer_good;
            }
        }
mpc_pointer_good:
        if (mpc == nullptr || !mpc->valid()) {
            get_klogger() << "MP config table is bad\n";
            goto mpc_pointer_bad;
        }
        mp_tbl_hdr = *mpc;

        {
            const mp_any_entry *entry = mpc->first_entry();
            for (int i = 0; i < mpc->table_entries; i++) {
                switch (entry->entry_type) {
                    case 0:
                    {
                        const auto &cpu = entry->cpu();
                        if (ncpu < MAX_CPUs) {
                            get_klogger() << "cpu" << ncpu << ": flags=" << cpu.cpu_flags <<
                            " signature=" << cpu.cpu_signature << " features=" <<
                            cpu.features << " apic-id=" << cpu.local_apic_id <<
                            " apic-v=" << cpu.local_apic_version << "\n";
                            cpus[ncpu++] = cpu;
                        }
                    }
                        break;
                    case 1:
                    {
                        const auto &bus = entry->bus();
                        if (nbus < MAX_BUSs) {
                            char bus_type[7];
                            for (int i = 0; i < 6; i++) {
                                bus_type[i] = bus.bus_type[i];
                            }
                            bus_type[6] = 0;
                            get_klogger() << "bus" << nbus << ": id=" << bus.bus_id << " " << &(bus_type[0]) << "\n";
                            this->bus[nbus++] = bus;
                        }
                    }
                        break;
                    case 2:
                    {
                        const auto &ioapic = entry->ioapic();
                        if (nioapic < MAX_IOAPICs) {
                            get_klogger() << "ioapic" << nioapic << ": flags=" << ioapic.ioapic_flags << " id=" << ioapic.ioapic_id << " version=" << ioapic.ioapic_version << " mmap="<< ioapic.mapped_memory_addr << "\n";
                            this->ioapic[nioapic++] = ioapic;
                        }
                    }
                        break;
                    case 3:
                    {
                        const auto &ioapic_int = entry->ioapic_int();
                        if (n_ioapic_int < MAX_INTs) {
                            //get_klogger() << "ioapic-interrupt" << n_ioapic_int << ": ioapic-id=" << ioapic_int.destination_ioapic_id
                            //<< " flags=" << ioapic_int.interrupt_flags << " type=" << ioapic_int.interrupt_type << " int="
                            //<< ioapic_int.ioapic_intin << " bus-id=" << ioapic_int.source_bus_id << " irq="
                            //<< ioapic_int.source_bus_irq << "\n";
                            this->ioapic_ints[n_ioapic_int++] = ioapic_int;
                        }
                    }
                        break;
                    case 4:
                    {
                        const auto &local_int = entry->local_int();
                        if (n_local_int < MAX_LOCAL_INTs) {
                            //get_klogger() << "local-interrupt" << n_local_int << ": flags=" << local_int.interrupt_flags
                            //<< " type=" << local_int.interrupt_type << " apic-id=" << local_int.destination_apic_id
                            //<< " int=" << local_int.apic_lintin << " bus-id=" << local_int.source_bus_id
                            //<< " irq=" << local_int.source_bus_irq << "\n";
                            this->local_ints[n_local_int++] = local_int;
                        }
                    }
                        break;
                    default:
                        get_klogger() << "mp conf entry: type=" << entry->entry_type << " size="
                        << entry->entry_length << "\n";
                }
                entry = entry->next();
            }
        }
        valid = true;

mpc_pointer_bad:
        ;
    }
}

/*int cpu_mpfp::get_current_cpu_num() const {
    cpuid<1> q{};
    uint8_t apic_id = q.get_apic_id();
    get_klogger() << "Current cpu apic id " << apic_id << "\n";
    for (int i = 0; i < ncpu; i++) {
        // Seems to not match on real hw
        if (cpus[i].local_apic_id == apic_id) {
            return i;
        }
    }
    return -1;
}*/

bool mp_floating_pointer::valid() {
    if (length != 1) {
        get_klogger() << "mp floating pointer length error\n";
        return false;
    }
    if (spec_rev == 0) {
        get_klogger() << "mp floating pointer spec revision error\n";
        return false;
    }
    uint8_t *bptr = (uint8_t *) (void *) this;
    int length = this->length;
    length = length * 16;
    uint8_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += bptr[i];
    }
    if (sum == 0) {
        return true;
    } else {
        get_klogger() << "mp floating pointer checksum\n";
        return false;
    }
}

bool mp_configuration_table_header::valid() {
    if (signature != 0x504D4350) {
        get_klogger() << "bad mpc checksum\n";
        return false;
    }
    {
        uint8_t *bptr = (uint8_t *) (void *) this;
        if (tbl_length > 0x1000) {
            get_klogger() << "mpc table too long for allocation\n";
            return false;
        }
        uint8_t sum = 0;
        for (uint16_t i = 0; i < tbl_length; i++) {
            sum += bptr[i];
        }
        if (sum != 0) {
            get_klogger() << "mpc table checksum error\n";
            return false;
        }
    }
    get_klogger() << "mpc table checksum ok\n";
    return true;
}
