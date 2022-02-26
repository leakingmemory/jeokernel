//
// Created by sigsegv on 2/26/22.
//

#ifndef JEOKERNEL_USTORAGE_STRUCTS_H
#define JEOKERNEL_USTORAGE_STRUCTS_H

#include "usb_control_req.h"

struct ustorage_mass_reset : usb_control_request {
    ustorage_mass_reset(uint16_t interface) : usb_control_request(
        RequestType(usb_control_direction::HOST_TO_DEVICE, usb_control_request_type::CLASS, usb_control_recepient::INTERFACE),
        0xFF, 0, interface, 0
    ) {}
};

struct ustorage_get_max_lun : usb_control_request {
    ustorage_get_max_lun(uint16_t interface) : usb_control_request(
        RequestType(usb_control_direction::DEVICE_TO_HOST, usb_control_request_type::CLASS, usb_control_recepient::INTERFACE),
        0xFE, 0, interface, 1
    ) {}
};

#endif //JEOKERNEL_USTORAGE_STRUCTS_H
