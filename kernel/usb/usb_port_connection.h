//
// Created by sigsegv on 9/10/21.
//

#ifndef JEOKERNEL_USB_PORT_CONNECTION_H
#define JEOKERNEL_USB_PORT_CONNECTION_H

#include <devices/devices.h>
#include "usb_control_req.h"
#include "control_request_trait.h"
#include "usb_types.h"
#include "usb_hub.h"
#include <thread>

template <int n> struct usb_byte_buffer {
    uint8_t data[n];
};

class usb_hw_enumerated_device {
public:
    virtual ~usb_hw_enumerated_device() {
    }
    virtual void Stop() = 0;
    virtual usb_speed Speed() const = 0;
    virtual uint8_t SlotId() const = 0;
    virtual usb_minimum_device_descriptor MinDesc() const = 0;
    virtual std::shared_ptr<usb_endpoint> Endpoint0() const = 0;
    virtual bool SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime) = 0;
    virtual bool SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir, int pollingIntervalMs) = 0;
};

class usb_hw_enumeration_addressing {
public:
    virtual ~usb_hw_enumeration_addressing() {
    }
    virtual std::shared_ptr<usb_hw_enumerated_device> set_address(uint8_t addr) = 0;
    virtual uint8_t get_address() = 0;
};

class usb_hw_enumeration {
public:
    virtual ~usb_hw_enumeration() {
    }
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
    virtual bool ClearStall() = 0;
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

class usb_port_connection : public control_request_trait {
private:
    usb_hub &hub;
    uint8_t port;
    std::shared_ptr<usb_func_addr> addr;
    uint8_t final_addr;
    usb_speed speed;
    std::thread *thread;
    std::shared_ptr<usb_endpoint> endpoint0;
    std::shared_ptr<usb_hw_enumerated_device> enumeratedDevice;
    usb_device_descriptor deviceDescriptor;
    Device *device;
    std::vector<UsbInterfaceInformation> interfaces;
    std::vector<uint8_t> portRouting;
public:
    usb_port_connection(usb_hub &hub, uint8_t port);
    ~usb_port_connection();
    void stop();
    uint8_t Port() {
        return port;
    }
    void start(std::shared_ptr<usb_hw_enumerated_device> enumeratedDevice);
    void SetDevice(Device &device) {
        this->device = &device;
    }
    std::shared_ptr<usb_endpoint> Endpoint0() {
        return endpoint0;
    }
    usb_speed Speed() {
        return speed;
    }
    uint8_t SlotId() {
        return enumeratedDevice->SlotId();
    }
    usb_hub &Hub() {
        return hub;
    }
    uint8_t Address() {
        return final_addr;
    }
    std::shared_ptr<usb_endpoint> InterruptEndpoint(int maxPacketSize, uint8_t endpointNum, usb_endpoint_direction direction, int pollingIntervalMs);
    bool ClearStall(uint8_t endpointNum, usb_endpoint_direction direction);
    const std::vector<UsbInterfaceInformation> &ReadConfigurations(const UsbDeviceInformation &devInfo);
    bool SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting);
    bool SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime);
};

#endif //JEOKERNEL_USB_PORT_CONNECTION_H
