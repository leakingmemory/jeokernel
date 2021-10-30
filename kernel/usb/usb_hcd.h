//
// Created by sigsegv on 8/18/21.
//

#ifndef JEOKERNEL_USB_HCD_H
#define JEOKERNEL_USB_HCD_H

#include <memory>
#include <concurrency/raw_semaphore.h>
#include "usb_port_connection.h"

class usb_hcd;

class usb_hcd_addr : public usb_func_addr {
private:
    usb_hcd &hcd;
    int addr;
public:
    usb_hcd_addr(usb_hcd &hcd, int addr) : usb_func_addr(), hcd(hcd), addr(addr) { }
    int GetAddr() const override {
        return addr;
    }
    ~usb_hcd_addr() override;
};

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

class usb_hcd : public usb_hub {
    friend usb_hcd_addr;
private:
    raw_semaphore hub_sema;
    std::vector<usb_port_connection *> connections;
    uint32_t func_addr_map[4];
public:
    usb_hcd() : hub_sema(0), connections(), func_addr_map{1, 0, 0, 0} {}
    virtual ~usb_hcd() {
        wild_panic("usb_hcd: delete not implemented");
    }
    virtual void dumpregs() = 0;
    virtual int GetNumberOfPorts() = 0;
    virtual uint32_t GetPortStatus(int port) = 0;
    virtual void SwitchPortOff(int port) = 0;
    virtual void SwitchPortOn(int port) = 0;
    virtual void ClearStatusChange(int port, uint32_t statuses) = 0;
    virtual std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override = 0;
    virtual size_t TransferBufferSize() = 0;
    virtual hw_spinlock &HcdSpinlock() = 0;
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
private:
    int AllocateFuncAddr();
    void ReleaseFuncAddr(int addr);
public:
    std::shared_ptr<usb_func_addr> GetFuncAddr() override {
        int addr = AllocateFuncAddr();
        if (addr > 0 && addr < 128) {
            std::shared_ptr<usb_func_addr> faddr{};
            faddr = std::make_shared<usb_hcd_addr>(*this, addr);
            return faddr;
        } else {
            return {};
        }
    }
};

#endif //JEOKERNEL_USB_HCD_H
