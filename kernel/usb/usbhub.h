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
    UsbInterfaceInformation usbInterfaceInformation;
    std::shared_ptr<usb_endpoint> endpoint0;
    std::shared_ptr<usb_endpoint> poll_endpoint;
    std::shared_ptr<usb_transfer> poll_transfer;
    usb_endpoint_descriptor endpoint;
    uint64_t transfercount;
    usb_hub_descr descr;
    bool individualPortPower;
    std::mutex ready_mtx;
    bool ready;
    bool disconnected;
public:
    usbhub(Bus &bus, const UsbInterfaceInformation &usbInterfaceInformation) : usb_hub("usbhub", bus), usbInterfaceInformation(usbInterfaceInformation), endpoint0(), poll_endpoint(), poll_transfer(),
                                                                               endpoint(), transfercount(0), descr(), individualPortPower(false), ready_mtx(), ready(false), disconnected(false) {}
    ~usbhub() override;
    void init() override;
    void stop() override;
    void RootHubStatusChange() override;
    void dumpregs() override;
    int GetNumberOfPorts() override;
    usb_speed HubSpeed() override;
    size_t TransferBufferSize() override;
    uint8_t HubSlotId() override;
    hw_spinlock &HcdSpinlock() override;
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
    std::shared_ptr<usb_hw_enumeration_addressing> EnumerateHubPort(const std::vector<uint8_t> &portRouting, usb_speed speed, usb_speed hubSpeed, uint8_t hubSlot) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    std::shared_ptr<usb_endpoint> CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_func_addr> GetFuncAddr() override;
    void PortRouting(std::vector<uint8_t> &route, uint8_t port) override;
    uint8_t GetHubAddress() override;

    void RegisterHub(usb_hub *child) override;
    void UnregisterHub(usb_hub *child) override;

    void interrupt();
};

class usbhub_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};

#endif //JEOKERNEL_USBHUB_H
