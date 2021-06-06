//
// Created by sigsegv on 02.05.2021.
//

#include <klogger.h>
#include <sstream>
#include "IOApic.h"

IOApic::IOApic(const cpu_mpfp &mpc) : vm(0x2000) {
    if (mpc.get_num_ioapics() < 1) {
        wild_panic("no ioapic");
    } else if (mpc.get_num_ioapics() > 1) {
        wild_panic("more than one ioapic");
    }
    uint64_t paddr = mpc.get_ioapic(0).mapped_memory_addr;
    get_klogger() << "ioapic at addr " << paddr;
    uint64_t offset = paddr & 0xFFF;
    uint64_t end_addr = paddr + 0x1F;
    paddr -= offset;
    get_klogger() << " -> " << paddr << "+" << offset << " ";
    uint64_t next_page = paddr + 0x1000;
    vm.page(0).rwmap(paddr, true, true);
    get_klogger() << "M";
    if (end_addr >= next_page) {
        get_klogger() << "M";
        vm.page(1).rwmap(next_page, true, true);
    }
    uint8_t *ptr = (uint8_t *) vm.pointer();
    get_klogger() << " v-> " << (uint64_t) ptr;
    ptr += offset;
    get_klogger() << "->"<<(uint64_t) ptr << "\n";
    this->pointer = (uint32_t *) (void *) ptr;
}

void
IOApic::enable(uint8_t vector, uint8_t destination_id, bool enable, bool logical_destination, uint8_t delivery_mode, bool active_low,
               bool level_triggered) {
    uint32_t mask = 0xFFFE50FF;
    uint32_t set = delivery_mode;
    if (logical_destination) {
        set |= 1 << 3;
    }
    if (active_low) {
        set |= 1 << 5;
    }
    if (level_triggered) {
        set |= 1 << 7;
    }
    if (!enable) {
        set |= 1 << 8;
    }
    set = set << 8;
    uint8_t reg = (vector * 2) + 0x10;
    uint32_t prev_value = (*this)[reg];
    uint32_t value{prev_value & mask};
    value |= set;
    uint32_t prev_value2 = (*this)[reg+1];
    uint32_t value2{prev_value2 & 0x00FFFFFF};
    value2 |= ((uint32_t) destination_id) << 24;
    (*this)[reg] = value;
    (*this)[reg+1] = value2;

    std::stringstream msg;
    msg << "IOApic int set " << std::hex << (unsigned int) vector << " " << prev_value << ":" << prev_value2
        << " -> " << value << ":" << value2 << "\n";
    get_klogger() << msg.str().c_str();
}
