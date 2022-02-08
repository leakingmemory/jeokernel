//
// Created by sigsegv on 11/27/21.
//

#ifndef JEOKERNEL_XHCI_RESOURCES_32_H
#define JEOKERNEL_XHCI_RESOURCES_32_H

#include "xhci.h"
#include "Phys32Page.h"

class xhci_resources_32 : public xhci_resources {
private:
    Phys32Page dcbaa_page;
    std::unique_ptr<Phys32Page> scratchpad_array;
    std::vector<std::shared_ptr<Phys32Page>> scratchpad_pages;
    Phys32Page rings_page;
    StructPool<StructPoolAllocator<Phys32Page,usb_byte_buffer<XHCI_BUFFER_SHORT>>> bufPool;
public:
    xhci_resources_32(int maxScratchpadBuffers, uint16_t pagesize);
    ~xhci_resources_32() override;
    uint64_t DCBAAPhys() const override;
    xhci_dcbaa *DCBAA() const override;
    uint64_t ScratchpadPhys() const override;
    uint64_t CommandRingPhys() const override;
    uint64_t PrimaryFirstEventPhys() const override;
    uint64_t PrimaryEventSegmentsPhys() const override;
    xhci_rings *Rings() const override;
    std::shared_ptr<xhci_device> CreateDeviceData() const override;
    std::shared_ptr<usb_buffer> Alloc(size_t size) override;
};

class xhci_device_32 : public xhci_device {
private:
    Phys32Page page;
public:
    xhci_device_32();
    ~xhci_device_32() override;
    xhci_slot_data *SlotData() const override;
    uint64_t Endpoint0RingPhys() const override;
    uint64_t SlotContextPhys() const override;
};


#endif //JEOKERNEL_XHCI_RESOURCES_32_H
