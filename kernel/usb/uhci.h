//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_UHCI_H
#define JEOKERNEL_UHCI_H

#include <devices/drivers.h>
#include "../pci/pci.h"
#include "Phys32Page.h"
#include "StructPool.h"

struct uhci_queue_head {
    uint32_t head;
    uint32_t element;
    uint32_t unused1;
    uint32_t unused2;
};
static_assert(sizeof(uhci_queue_head) == 16);

struct uhci_transfer_descriptor {
    uint32_t next;
    uint16_t ActLen;
    uint16_t Status;
    uint8_t PID;
    uint8_t DeviceAddress : 7;
    uint8_t Endpoint : 4;
    uint8_t DataToggle : 1;
    uint8_t Reserved : 1;
    uint16_t MaxLen : 11;
    uint32_t BufferPointer;
} __attribute__((__packed__));
static_assert(sizeof(uhci_transfer_descriptor) == 16);

union uhci_qh_or_td {
    uhci_queue_head qh;
    uhci_transfer_descriptor td;
};
static_assert(sizeof(uhci_qh_or_td) == 16);

class uhci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
    Phys32Page FramesPhys;
    uint32_t *Frames;
    StructPool<StructPoolAllocator<Phys32Page,uhci_qh_or_td>> qhtdPool;
    std::shared_ptr<StructPoolPointer<uhci_qh_or_td,uint32_t>> qh;
    uint32_t iobase;
public:
    uhci(Bus &bus, PciDeviceInformation &deviceInformation) :
        Device("uhci", &bus), pciDeviceInformation(deviceInformation),
        FramesPhys(4096), Frames((uint32_t *) FramesPhys.Pointer()),
        qhtdPool(), qh() {}
    void init() override;
private:
    bool irq();
};

class uhci_driver : public Driver {
public:
    uhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_UHCI_H
