//
// Created by sigsegv on 12/11/21.
//

#ifndef JEOKERNEL_XHCI_TRANSFER_H
#define JEOKERNEL_XHCI_TRANSFER_H

#include "usb_port_connection.h"
#include "usb_transfer.h"

class xhci_transfer : public usb_transfer {
private:
    std::shared_ptr<usb_buffer> buffer;
    uint64_t physAddr;
    usb_transfer_status status;
public:
    xhci_transfer(std::shared_ptr<usb_buffer> buffer, uint64_t physAddr, std::size_t size) : usb_transfer(size), buffer(buffer), physAddr(physAddr), status(usb_transfer_status::NOT_ACCESSED) { }
    std::shared_ptr<usb_buffer> Buffer() override;
    usb_transfer_status GetStatus() override;
    void SetStatus(uint8_t status, std::size_t length);
};


#endif //JEOKERNEL_XHCI_TRANSFER_H
