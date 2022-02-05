//
// Created by sigsegv on 1/31/22.
//

#ifndef JEOKERNEL_USB_HUB_H
#define JEOKERNEL_USB_HUB_H

#include <devices/devices.h>
#include <string>
#include "usb_types.h"
#include "concurrency/hw_spinlock.h"

#define USB_PORT_STATUS_CCS    0x000001 // Current Connection Status
#define USB_PORT_STATUS_PES    0x000002 // Port Enable Status
#define USB_PORT_STATUS_PSS    0x000004 // Port Suspend Status
#define USB_PORT_STATUS_POCI   0x000008 // Port Over Current Indicator
#define USB_PORT_STATUS_PRS    0x000010 // Port Reset Status
#define USB_PORT_STATUS_PPS    0x000020 // Port Power Status
#define USB_PORT_STATUS_LSDA   0x000040 // Low Speed Device Attached
#define USB_PORT_STATUS_CSC    0x000080 // Connect Status Change
#define USB_PORT_STATUS_PESC   0x000100 // Port Enable Status Change
#define USB_PORT_STATUS_PSSC   0x000200 // Port Suspend Status Change
#define USB_PORT_STATUS_OCIC   0x000400 // Port Over Current Indicator Change
#define USB_PORT_STATUS_PRSC   0x000800 // Port Reset Status Change
#define USB_PORT_STATUS_RESUME 0x001000 // Resume detected

class usb_endpoint;
class usb_hw_enumeration;
class usb_hw_enumeration_addressing;
class usb_func_addr;
class usb_port_connection;

class usb_hub : public Bus {
private:
    std::vector<usb_port_connection *> connections;
public:
    explicit usb_hub(std::string hubType, Bus &parentBus) : Bus(hubType, &parentBus), connections() { }
    ~usb_hub() override;
    virtual void RootHubStatusChange() = 0;
    void stop() override;
    virtual void dumpregs() = 0;
    virtual int GetNumberOfPorts() = 0;
    virtual usb_speed HubSpeed() = 0;
    virtual uint8_t HubSlotId() {
        return 0;
    }
    virtual hw_spinlock &HcdSpinlock() = 0;
    virtual uint32_t GetPortStatus(int port) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) = 0;
    virtual void SwitchPortOff(int port) = 0;
    virtual void SwitchPortOn(int port) = 0;
    virtual void ClearStatusChange(int port, uint32_t statuses) = 0;
    virtual void EnablePort(int port) = 0;
    virtual void DisablePort(int port) = 0;
    virtual void ResetPort(int port) = 0;
    virtual bool ResettingPort(int port) = 0;
    virtual bool EnabledPort(int port) = 0;
    virtual usb_speed PortSpeed(int port) = 0;
    virtual std::shared_ptr<usb_hw_enumeration> EnumeratePort(int port) {
        return {};
    }
    virtual std::shared_ptr<usb_hw_enumeration_addressing> EnumerateHubPort(const std::vector<uint8_t> &portRouting, usb_speed speed, usb_speed hubSpeed, uint8_t hubSlot) {
        return {};
    }
    virtual std::shared_ptr<usb_func_addr> GetFuncAddr() = 0;
    virtual bool PortResetEnablesPort() {
        return true;
    }
    virtual void PortRouting(std::vector<uint8_t> &route, uint8_t port) = 0;
    virtual uint8_t GetHubAddress() = 0;

    virtual void RegisterHub(usb_hub *child) = 0;
    virtual void UnregisterHub(usb_hub *child) = 0;

    void PortConnected(uint8_t port);
    void PortDisconnected(uint8_t port);

    bool InitHubPorts();
    void RunPollPorts();
};

#endif //JEOKERNEL_USB_HUB_H
