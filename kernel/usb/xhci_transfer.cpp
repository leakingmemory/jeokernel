//
// Created by sigsegv on 12/11/21.
//

#include "xhci_transfer.h"

std::shared_ptr<usb_buffer> xhci_transfer::Buffer() {
    return buffer;
}

usb_transfer_status xhci_transfer::GetStatus() {
    return status;
}
