//
// Created by sigsegv on 9/10/21.
//

#ifndef JEOKERNEL_USB_PORT_CONNECTION_H
#define JEOKERNEL_USB_PORT_CONNECTION_H

class usb_hub {
public:
    virtual void EnablePort(int port) = 0;
    virtual void DisablePort(int port) = 0;
    virtual void ResetPort(int port) = 0;
    virtual bool ResettingPort(int port) = 0;
    virtual bool EnabledPort(int port) = 0;
};

class usb_port_connection {
private:
    usb_hub &hub;
    uint8_t port;
public:
    usb_port_connection(usb_hub &hub, uint8_t port);
    ~usb_port_connection();
    uint8_t Port() {
        return port;
    }
};

#endif //JEOKERNEL_USB_PORT_CONNECTION_H
