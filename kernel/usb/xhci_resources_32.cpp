//
// Created by sigsegv on 11/27/21.
//

#include <strings.h>
#include "xhci_resources_32.h"
#include "UsbBuffer32.h"

xhci_resources_32::xhci_resources_32(int maxScratchpadBuffers, uint16_t pagesize) :
        dcbaa_page(4096), scratchpad_array(), scratchpad_pages(), rings_page(sizeof(xhci_rings)), bufPool() {
    if (pagesize > 4096) {
        wild_panic("Pagesize other than 4096 is not supported by driver due to alignment issues.");
    }
    if (maxScratchpadBuffers > 0) {
        scratchpad_array = std::make_unique<Phys32Page>(maxScratchpadBuffers * sizeof(uint64_t));
        scratchpad_pages.reserve(maxScratchpadBuffers);
        uint64_t *buffers = (uint64_t *) scratchpad_array->Pointer();
        for (int i = 0; i < maxScratchpadBuffers; i++) {
            scratchpad_pages.emplace_back(new Phys32Page(pagesize));
            buffers[i] = scratchpad_pages[i]->PhysAddr();
            bzero(scratchpad_pages[i]->Pointer(), pagesize);
        }
    }
    new (rings_page.Pointer()) xhci_rings(rings_page.PhysAddr());
}

uint64_t xhci_resources_32::DCBAAPhys() const {
    return dcbaa_page.PhysAddr();
}

xhci_dcbaa *xhci_resources_32::DCBAA() const {
    return (xhci_dcbaa *) dcbaa_page.Pointer();
}

uint64_t xhci_resources_32::ScratchpadPhys() const {
    return scratchpad_array ? scratchpad_array->PhysAddr() : 0;
}

xhci_resources_32::~xhci_resources_32() {
    ((xhci_rings *) rings_page.Pointer())->~xhci_rings();
}

uint64_t xhci_resources_32::CommandRingPhys() const {
    return rings_page.PhysAddr();
}

xhci_rings *xhci_resources_32::Rings() const {
    return (xhci_rings *) rings_page.Pointer();
}

uint64_t xhci_resources_32::PrimaryFirstEventPhys() const {
    return rings_page.PhysAddr() + Rings()->PrimaryEventRingOffset();
}

uint64_t xhci_resources_32::PrimaryEventSegmentsPhys() const {
    return rings_page.PhysAddr() + Rings()->EventSegmentsOffset();
}

std::shared_ptr<xhci_device> xhci_resources_32::CreateDeviceData() const {
    std::shared_ptr<xhci_device> deviceData{new xhci_device_32};
    return deviceData;
}

std::shared_ptr<usb_buffer> xhci_resources_32::Alloc(size_t size) {
    if (size <= XHCI_BUFFER_SHORT) {
        std::shared_ptr<usb_buffer> buffer{new xhci_buffer<XHCI_BUFFER_SHORT,uint32_t>(bufPool.Alloc())};
        return buffer;
    } else {
        std::shared_ptr<usb_buffer> buffer{new UsbBuffer32(size)};
        return buffer;
    }
}

#define XHCI_DEVICE_SLOT_SIZE 4096
static_assert(sizeof(xhci_slot_data) <= XHCI_DEVICE_SLOT_SIZE);
xhci_device_32::xhci_device_32() : page(XHCI_DEVICE_SLOT_SIZE) {
    bzero(page.Pointer(), XHCI_DEVICE_SLOT_SIZE);
    new (page.Pointer()) xhci_slot_data(page.PhysAddr());
}

xhci_device_32::~xhci_device_32() {
    ((xhci_slot_data *) page.Pointer())->~xhci_slot_data();
}

xhci_slot_data *xhci_device_32::SlotData() const {
    return (xhci_slot_data *) page.Pointer();
}

uint64_t xhci_device_32::Endpoint0RingPhys() const {
    return page.PhysAddr() + ((xhci_slot_data *) page.Pointer())->Endpoint0RingOffset();
}

uint64_t xhci_device_32::SlotContextPhys() const {
    return page.PhysAddr() + ((xhci_slot_data *) page.Pointer())->SlotContextOffset();
}
