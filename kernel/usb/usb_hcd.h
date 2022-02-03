//
// Created by sigsegv on 8/18/21.
//

#ifndef JEOKERNEL_USB_HCD_H
#define JEOKERNEL_USB_HCD_H

#include <memory>
#include <concurrency/mul_semaphore.h>
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

class usb_hcd : public usb_hub {
    friend usb_hcd_addr;
private:
    mul_semaphore hub_sema;
    uint32_t func_addr_map[4];
    std::mutex children_mtx;
    std::vector<usb_hub *> new_children;
    std::vector<usb_hub *> children;
public:
    usb_hcd(std::string hcdType, Bus &parentBus) : usb_hub(hcdType, parentBus), hub_sema(), func_addr_map{1, 0, 0, 0}, children_mtx(), new_children(), children() {}
    virtual ~usb_hcd() {
        wild_panic("usb_hcd: delete not implemented");
    }
    virtual int GetNumberOfPorts() = 0;
    virtual size_t TransferBufferSize() = 0;
    virtual hw_spinlock &HcdSpinlock() = 0;
private:
    [[noreturn]] void BusInit();
public:
    void Run();
    void RootHubStatusChange();
    virtual bool PollPorts();
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
    void PortRouting(std::vector<uint8_t> &route, uint8_t port) override;
    uint8_t GetHubAddress() override;

    void RegisterHub(usb_hub *child) override;
    void UnregisterHub(usb_hub *child) override;
};

#endif //JEOKERNEL_USB_HCD_H
