//
// Created by sigsegv on 9/10/21.
//

#include <klogger.h>
#include "usb_port_connection.h"

usb_port_connection::usb_port_connection(uint8_t port) : port(port) {
    get_klogger() << "USB port connected\n";
}

usb_port_connection::~usb_port_connection() {
    get_klogger() << "USB port disconnected\n";
}
