//
// Created by sigsegv on 10/11/21.
//

#ifndef JEOKERNEL_USB_CONTROL_REQ_H
#define JEOKERNEL_USB_CONTROL_REQ_H

#include <stdint.h>

struct usb_control_request {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;

    usb_control_request(
                uint8_t bmRequestType,
                uint8_t bRequest,
                uint16_t wValue,
                uint16_t wIndex,
                uint16_t wLength
            ) :
            bmRequestType(bmRequestType),
            bRequest(bRequest),
            wValue(wValue),
            wIndex(wIndex),
            wLength(wLength) {}
} __attribute__((__packed__));

struct usb_set_address : usb_control_request {
    usb_set_address(uint8_t addr) : usb_control_request(0, 5, addr, 0, 0) {}
};


#endif //JEOKERNEL_USB_CONTROL_REQ_H
