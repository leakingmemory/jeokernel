//
// Created by sigsegv on 7/25/21.
//

#ifndef JEOKERNEL_PCI_IRQ_RT_H
#define JEOKERNEL_PCI_IRQ_RT_H

struct PciIRQRouting {
    uint64_t Address;
    uint32_t Pin;
    uint32_t SourceIndex;
    std::string Source;
};

#endif //JEOKERNEL_PCI_IRQ_RT_H
