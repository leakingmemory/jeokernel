//
// Created by sigsegv on 9/10/21.
//

#ifndef JEOKERNEL_USB_PORT_CONNECTION_H
#define JEOKERNEL_USB_PORT_CONNECTION_H

enum usb_transfer_direction {
    IN,
    OUT,
    BOTH
};

enum usb_speed {
    LOW,
    FULL
};

enum usb_endpoint_type {
    CONTROL
};

class usb_endpoint {
public:
    virtual ~usb_endpoint() {}
};

class usb_hub {
public:
    virtual std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_transfer_direction dir, usb_speed speed) = 0;
    virtual void EnablePort(int port) = 0;
    virtual void DisablePort(int port) = 0;
    virtual void ResetPort(int port) = 0;
    virtual bool ResettingPort(int port) = 0;
    virtual bool EnabledPort(int port) = 0;
    virtual usb_speed PortSpeed(int port) = 0;
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
