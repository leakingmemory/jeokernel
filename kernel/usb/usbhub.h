//
// Created by sigsegv on 1/30/22.
//

#ifndef JEOKERNEL_USBHUB_H
#define JEOKERNEL_USBHUB_H

#include <cstdint>
#include <devices/drivers.h>
#include "usb_port_connection.h"
#include "usb_hcd.h"

struct usb_hub_descr {
    uint8_t length;
    uint8_t type;
    uint8_t portCount;
    uint16_t flags;
    uint8_t portPowerTime;
    uint8_t current;
} __attribute__((__packed__));

class usbhub : public usb_hub {
private:
    UsbDeviceInformation usbDeviceInformation;
    std::shared_ptr<usb_endpoint> endpoint0;
    usb_hub_descr descr;
    bool individualPortPower;
public:
    usbhub(Bus &bus, const UsbDeviceInformation &usbDeviceInformation) : usb_hub("usbhub", bus), usbDeviceInformation(usbDeviceInformation), endpoint0(), descr(), individualPortPower(false) {}
    ~usbhub() override;
    void init() override;

    void dumpregs() override;
    int GetNumberOfPorts() override;
    uint32_t GetPortStatus(int port) override;
    void SwitchPortOff(int port) override;
    void SwitchPortOn(int port) override;
    void ClearStatusChange(int port, uint32_t statuses) override;
    void EnablePort(int port) override;
    void DisablePort(int port) override;
    void ResetPort(int port) override;
    bool ResettingPort(int port) override;
    bool EnabledPort(int port) override;
    usb_speed PortSpeed(int port) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    std::shared_ptr<usb_func_addr> GetFuncAddr() override;
};

class usbhub_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};

#endif //JEOKERNEL_USBHUB_H
