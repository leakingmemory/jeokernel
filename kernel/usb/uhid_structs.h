//
// Created by sigsegv on 11/1/21.
//

#ifndef JEOKERNEL_UHID_STRUCTS_H
#define JEOKERNEL_UHID_STRUCTS_H

#include "usb_control_req.h"

#define UHID_PROTO_BOOT   0
#define UHID_PROTO_REPORT 1
struct uhid_set_protocol : usb_control_request {
    uhid_set_protocol(uint8_t proto) : usb_control_request(0x21, 0x0B, proto, 0, 0) { }
};

struct uhid_get_report : usb_control_request {
    uhid_get_report(uint8_t length) : usb_control_request(0xA1, 1, 0x100, 0, length) { }
    uhid_get_report() : uhid_get_report(8) { }
};

#endif //JEOKERNEL_UHID_STRUCTS_H
