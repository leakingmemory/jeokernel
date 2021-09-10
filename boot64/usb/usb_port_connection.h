//
// Created by sigsegv on 9/10/21.
//

#ifndef JEOKERNEL_USB_PORT_CONNECTION_H
#define JEOKERNEL_USB_PORT_CONNECTION_H

class usb_port_connection {
private:
    uint8_t port;
public:
    usb_port_connection(uint8_t port);
    ~usb_port_connection();
    uint8_t Port() {
        return port;
    }
};

#endif //JEOKERNEL_USB_PORT_CONNECTION_H
