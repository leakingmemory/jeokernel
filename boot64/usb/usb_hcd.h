//
// Created by sigsegv on 8/18/21.
//

#ifndef JEOKERNEL_USB_HCD_H
#define JEOKERNEL_USB_HCD_H

#include <memory>
#include <concurrency/raw_semaphore.h>
#include "usb_port_connection.h"

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

class usb_hcd : usb_hub {
private:
    raw_semaphore hub_sema;
    std::vector<usb_port_connection *> connections;
public:
    usb_hcd() : hub_sema(0), connections() {}
    virtual ~usb_hcd() {
        wild_panic("usb_hcd: delete not implemented");
    }
    virtual int GetNumberOfPorts() = 0;
    virtual uint32_t GetPortStatus(int port) = 0;
    virtual void SwitchPortOff(int port) = 0;
    virtual void SwitchPortOn(int port) = 0;
    virtual void ClearStatusChange(int port, uint32_t statuses) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_transfer_direction dir, usb_speed speed) override = 0;
private:
    [[noreturn]] void BusInit();
    void PortConnected(uint8_t port);
    void PortDisconnected(uint8_t port);
public:
    void Run();
    void RootHubStatusChange();
    virtual void EnablePort(int port) override = 0;
    virtual void DisablePort(int port) override = 0;
    virtual void ResetPort(int port) override = 0;
    bool EnabledPort(int port) override;
    bool ResettingPort(int port) override;
    virtual usb_speed PortSpeed(int port) override = 0;
};

#endif //JEOKERNEL_USB_HCD_H
