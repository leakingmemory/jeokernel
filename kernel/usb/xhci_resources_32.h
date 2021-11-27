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
public:
    xhci_resources_32(int maxScratchpadBuffers, uint16_t pagesize);
    uint64_t DCBAAPhys() const override;
    xhci_dcbaa *DCBAA() const override;
    uint64_t ScratchpadPhys() const override;
};


#endif //JEOKERNEL_XHCI_RESOURCES_32_H
