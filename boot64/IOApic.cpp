//
// Created by sigsegv on 02.05.2021.
//

#include <klogger.h>
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
    vm.page(0).rwmap(paddr);
    get_klogger() << "M";
    if (end_addr >= next_page) {
        get_klogger() << "M";
        vm.page(1).rwmap(next_page);
    }
    uint8_t *ptr = (uint8_t *) vm.pointer();
    get_klogger() << " v-> " << (uint64_t) ptr;
    ptr += offset;
    get_klogger() << "->"<<(uint64_t) ptr << "\n";
    this->pointer = (uint32_t *) (void *) ptr;
}
