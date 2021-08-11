//
// Created by sigsegv on 7/25/21.
//

#ifndef JEOKERNEL_PCI_IRQ_RT_H
#define JEOKERNEL_PCI_IRQ_RT_H

struct PciIRQRouting {
    union {
        uint64_t Address;
        struct {
            uint16_t Func;
            uint16_t Slot;
            uint32_t res_zero;
        } __attribute__((__packed__));
    };
    uint32_t Pin;
    uint32_t SourceIndex;
    std::string Source;
};

struct IRQLink {
    uint8_t              ProducerConsumer;
    uint8_t              Triggering;
    uint8_t              Polarity;
    uint8_t              Shareable;
    uint8_t              WakeCapable;
    uint8_t              InterruptCount;
    std::string          ResourceSource;
    std::vector<uint8_t> Interrupts;

    uint8_t operator [](size_t vector) {
        if (vector < Interrupts.size()) {
            return (Interrupts.data())[vector];
        } else {
            return 0;
        }
    }
    size_t size() {
        return Interrupts.size();
    }
};

#endif //JEOKERNEL_PCI_IRQ_RT_H
