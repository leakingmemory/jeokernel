//
// Created by sigsegv on 02.05.2021.
//

#include <klogger.h>
#include <core/LocalApic.h>

LocalApic::LocalApic(const cpu_mpfp &mpc) : vm(0x2000) {
    uint64_t paddr = mpc.get_local_apic_addr();
    //get_klogger() << "lapic at addr " << paddr;
    uint64_t offset = paddr & 0xFFF;
    uint64_t end_addr = paddr + 0x3FF;
    paddr -= offset;
    //get_klogger() << " -> " << paddr << "+" << offset << " ";
    uint64_t next_page = paddr + 0x1000;
    vm.page(0).rwmap(paddr);
    //get_klogger() << "M";
    if (end_addr >= next_page) {
        //get_klogger() << "M";
        vm.page(1).rwmap(next_page);
    }
    uint8_t *ptr = (uint8_t *) vm.pointer();
    //get_klogger() << " v-> " << (uint64_t) ptr;
    ptr += offset;
    //get_klogger() << "->"<<(uint64_t) ptr << "\n";
    this->pointer = (uint32_t *) (void *) ptr;
}

uint64_t LocalApic::get_msr_base_reg() {
    uint64_t msr = LAPIC_BASE_MSR;
    uint32_t low;
    uint32_t high;
    asm("mov %2, %%rcx; rdmsr; mov %%eax, %0; mov %%edx, %1" : "=r"(low), "=r"(high) : "r"(msr) : "%rax", "%rcx", "%rdx");
    uint64_t val = high;
    val = val << 32;
    val += low;
    return val;
}

void LocalApic::set_msr_base_reg(uint64_t val) {
    uint64_t msr = LAPIC_BASE_MSR;
    uint64_t low = val & 0x00000000FFFFFFFF;
    uint64_t high = (val >> 32) & 0x00000000FFFFFFFF;
    asm("mov %0, %%rcx; mov %1, %%rax; mov %2, %%rdx; wrmsr" :: "r"(msr), "r"(low), "r"(high) : "%rax","%rcx","%rdx");
}
