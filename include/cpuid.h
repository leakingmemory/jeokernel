//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_CPUID_H
#define JEOKERNEL_CPUID_H

#include <cstdint>
#include <type_traits>

#define X86_APIC_BASE_MSR 0x1B
#define X86_APIC_BASE_MSR_BSP 0x100
#define X86_APIC_BASE_MSR_ENABLE 0x800

#define CPUID_EDX_APIC_AVAIL

template <uint32_t query> class cpuid {
private:
    uint32_t eax, ebx, ecx, edx;
public:
    cpuid() {
        asm(""
            "mov %4, %%eax;"
            "cpuid;"
            "mov %%eax, %0;"
            "mov %%ebx, %1;"
            "mov %%ecx, %2;"
            "mov %%edx, %3"
        :"=r"(eax),"=r"(ebx),"=r"(ecx),"=r"(edx)
        :"r"(query)
        :"%rax","%rbx","%rcx","%rdx");
    }
    /**
     * Query 1, APIC support?
     * @return bool
     */
    /*constexpr std::enable_if<query == 1,bool>::type cpu_apic_support() const {
        if (query == 1) {
            return CPUID_EDX_APIC_AVAIL;
        }
    }*/

    /**
     * Query 1, CPU ID
     * @return uint8_t
     */
    constexpr typename std::enable_if<query == 1,uint8_t>::type get_apic_id() const {
        if (query == 1) {
            return ebx >> 24;
        }
    }

    constexpr typename std::enable_if<query == 1,uint32_t>::type get_cpu_signature() const {
        if (query == 1) {
            return edx & 0xFFF;
        }
    }
};

#endif //JEOKERNEL_CPUID_H
