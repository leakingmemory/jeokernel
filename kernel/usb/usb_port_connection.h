//
// Created by sigsegv on 9/10/21.
//

#ifndef JEOKERNEL_USB_PORT_CONNECTION_H
#define JEOKERNEL_USB_PORT_CONNECTION_H

#include "usb_control_req.h"

template <int n> struct usb_byte_buffer {
    uint8_t data[n];
};

enum class usb_transfer_direction {
    SETUP,
    IN,
    OUT
};

enum class usb_endpoint_direction {
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

class usb_transfer;

#define TRANSFER_NO_INTERRUPT 0xFFFF

class usb_buffer;

class usb_endpoint {
public:
    virtual ~usb_endpoint() {}
    virtual bool Addressing64bit() = 0;
    virtual std::shared_ptr<usb_transfer> CreateTransfer(void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) = 0;
    virtual std::shared_ptr<usb_transfer> CreateTransfer(uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) = 0;
    virtual std::shared_ptr<usb_buffer> Alloc() = 0;
};

class usb_buffer {
public:
    virtual void *Pointer() = 0;
    virtual uint64_t Addr() = 0;
    virtual ~usb_buffer() {}
    void CopyTo(void *ptr, size_t size) {
        memcpy(ptr, Pointer(), size);
    }
    template <typename T> void CopyTo(T &t) {
        CopyTo((void *) &t, sizeof(t));
    }
};

class usb_hub {
public:
    virtual void dumpregs() = 0;
    virtual uint32_t GetPortStatus(int port) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) = 0;
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
    std::shared_ptr<usb_buffer> ControlRequest(usb_endpoint &endpoint, const usb_control_request &request);
};

#endif //JEOKERNEL_USB_PORT_CONNECTION_H
