//
// Created by sigsegv on 9/10/21.
//

#ifndef JEOKERNEL_USB_PORT_CONNECTION_H
#define JEOKERNEL_USB_PORT_CONNECTION_H

#include <devices/devices.h>
#include "usb_control_req.h"
#include <thread>

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
    FULL,
    HIGH
};

enum class usb_endpoint_type {
    CONTROL,
    INTERRUPT
};

class usb_hw_enumeration_ready {
};

class usb_hw_enumeration_addressing {
public:
    virtual std::shared_ptr<usb_hw_enumeration_ready> set_address(uint8_t addr) = 0;
};

class usb_hw_enumeration {
public:
    virtual std::shared_ptr<usb_hw_enumeration_addressing> enumerate() = 0;
};

class usb_transfer;

#define TRANSFER_NO_INTERRUPT 0xFFFF

class usb_buffer;

class usb_endpoint {
public:
    virtual ~usb_endpoint() {}
    virtual bool Addressing64bit() = 0;
    virtual std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) = 0;
    virtual std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) = 0;
    virtual std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) = 0;
    virtual std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) = 0;
    virtual std::shared_ptr<usb_buffer> Alloc() = 0;
};

class usb_buffer {
public:
    virtual void *Pointer() = 0;
    virtual uint64_t Addr() = 0;
    virtual size_t Size() = 0;
    virtual ~usb_buffer() {}
    void CopyTo(void *ptr, size_t size) {
        memcpy(ptr, Pointer(), size);
    }
    void CopyTo(void *ptr, size_t offset, size_t size) {
        memcpy(ptr, ((uint8_t *) Pointer()) + offset, size);
    }
    template <typename T> void CopyTo(T &t) {
        CopyTo((void *) &t, sizeof(t));
    }
    template <typename T> void CopyTo(T &t, size_t offset) {
        CopyTo((void *) &t, offset, sizeof(t));
    }
};

class usb_func_addr {
public:
    usb_func_addr() { }
    usb_func_addr(const usb_func_addr &) = delete;
    usb_func_addr(usb_func_addr &&) = delete;
    usb_func_addr &operator = (const usb_func_addr &) = delete;
    usb_func_addr &operator = (usb_func_addr &&) = delete;

    virtual int GetAddr() const = 0;
    virtual ~usb_func_addr() { }
};

class usb_hub : public Bus {
public:
    explicit usb_hub(std::string hubType, Bus &parentBus) : Bus(hubType, &parentBus) { }
    virtual void dumpregs() = 0;
    virtual uint32_t GetPortStatus(int port) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) = 0;
    virtual void EnablePort(int port) = 0;
    virtual void DisablePort(int port) = 0;
    virtual void ResetPort(int port) = 0;
    virtual bool ResettingPort(int port) = 0;
    virtual bool EnabledPort(int port) = 0;
    virtual usb_speed PortSpeed(int port) = 0;
    virtual std::shared_ptr<usb_hw_enumeration> EnumeratePort(int port) {
        return {};
    }
    virtual std::shared_ptr<usb_func_addr> GetFuncAddr() = 0;
    virtual bool PortResetEnablesPort() {
        return true;
    }
};

class usb_port_connection;
struct UsbInterfaceInformation;

struct UsbDeviceInformation : public DeviceInformation {
    usb_port_connection &port;

    explicit UsbDeviceInformation(const usb_device_descriptor &devDesc, usb_port_connection &port);
    UsbDeviceInformation(const UsbDeviceInformation &cp);

    UsbDeviceInformation *GetUsbInformation() override;
    virtual UsbInterfaceInformation *GetUsbInterfaceInformation();
};

struct UsbIfacedevInformation;

struct UsbInterfaceInformation : public UsbDeviceInformation {
    usb_configuration_descriptor descr;
    usb_interface_descriptor iface;
    std::vector<usb_endpoint_descriptor> endpoints;

    UsbInterfaceInformation(
            const UsbDeviceInformation &devInfo,
            const usb_configuration_descriptor &descr,
            const usb_interface_descriptor &iface);
    UsbInterfaceInformation(const UsbInterfaceInformation &cp);

    UsbInterfaceInformation *GetUsbInterfaceInformation() override;
    virtual UsbIfacedevInformation *GetIfaceDev();
};

class usb_port_connection {
private:
    usb_hub &hub;
    uint8_t port;
    std::shared_ptr<usb_func_addr> addr;
    usb_speed speed;
    std::thread *thread;
    std::shared_ptr<usb_endpoint> endpoint0;
    usb_device_descriptor deviceDescriptor;
    Device *device;
    std::vector<UsbInterfaceInformation> interfaces;
public:
    usb_port_connection(usb_hub &hub, uint8_t port);
    ~usb_port_connection();
    uint8_t Port() {
        return port;
    }
    void start(usb_speed speed, const usb_minimum_device_descriptor &minDesc);
    void SetDevice(Device &device) {
        this->device = &device;
    }
    std::shared_ptr<usb_endpoint> Endpoint0() {
        return endpoint0;
    }
    std::shared_ptr<usb_buffer> ControlRequest(usb_endpoint &endpoint, const usb_control_request &request);
    bool ControlRequest(usb_endpoint &endpoint, const usb_control_request &request, void *data);
    std::shared_ptr<usb_endpoint> InterruptEndpoint(int maxPacketSize, uint8_t endpointNum, usb_endpoint_direction direction, int pollingIntervalMs);
    const std::vector<UsbInterfaceInformation> &ReadConfigurations(const UsbDeviceInformation &devInfo);
};

#endif //JEOKERNEL_USB_PORT_CONNECTION_H
