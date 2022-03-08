//
// Created by sigsegv on 8/18/21.
//

#include <sstream>
#include <chrono>
#include <thread>
#include "usb_hcd.h"

[[noreturn]] void usb_hcd::BusInit() {
    InitHubPorts();
    while (true) {
        hub_sema.acquire(1000);
        RunPollPorts();

        {
            std::vector<usb_hub *> incoming{};
            {
                std::lock_guard lock{children_mtx};
                for (usb_hub *hub : new_children) {
                    incoming.push_back(hub);
                }
                new_children.clear();
            }
            for (usb_hub *hub : incoming) {
                hub->InitHubPorts();
                children.push_back(hub);
            }
            incoming.clear();
            for (usb_hub *hub : children) {
                incoming.push_back(hub);
            }
            for (usb_hub *hub : incoming) {
                for (usb_hub *check : children) {
                    if (hub == check) {
                        hub->RunPollPorts();
                        break;
                    }
                }
            }
        }
    }
}

void usb_hcd::Run() {
    std::thread thread{[this] () {
        std::this_thread::set_name("[usbhcd]");
        this->BusInit();
    }};
    thread.detach();
}

void usb_hcd::RootHubStatusChange() {
    hub_sema.release();
}

bool usb_hcd::ResettingPort(int port) {
    return (GetPortStatus(port) & USB_PORT_STATUS_PRS) != 0;
}

bool usb_hcd::EnabledPort(int port) {
    return (GetPortStatus(port) & USB_PORT_STATUS_PES) != 0;
}

int usb_hcd::AllocateFuncAddr() {
    std::lock_guard lock{HcdSpinlock()};
    for (int i = 0; i < 4; i++) {
        if (func_addr_map[i] != 0) {
            for (int j = 0; j < 32; j++) {
                uint32_t bit = 1;
                bit = bit << j;
                if ((func_addr_map[i] & bit) == 0) {
                    func_addr_map[i] |= bit;
                    return (i * 32) + j;
                }
            }
        }
    }
    return -1;
}

void usb_hcd::ReleaseFuncAddr(int addr) {
    std::lock_guard lock{HcdSpinlock()};
    uint32_t bit = 1;
    bit = bit << (addr & 31);
    int i = addr / 32;
    func_addr_map[i] = func_addr_map[i] & (~bit);
}

bool usb_hcd::PollPorts() {
    return false;
}

void usb_hcd::RegisterHub(usb_hub *child) {
    std::lock_guard lock{children_mtx};
    new_children.push_back(child);
}

void usb_hcd::UnregisterHub(usb_hub *child) {
    {
        std::lock_guard lock{children_mtx};
        auto iterator = new_children.begin();
        while (iterator != new_children.end()) {
            if (*iterator == child) {
                new_children.erase(iterator);
                return;
            }
            ++iterator;
        }
    }
    auto iterator = children.begin();
    while (iterator != children.end()) {
        if (*iterator == child) {
            children.erase(iterator);
            break;
        }
        ++iterator;
    }
}

void usb_hcd::PortRouting(std::vector<uint8_t> &route, uint8_t port) {
    route.push_back(port);
}

uint8_t usb_hcd::GetHubAddress() {
    return 0;
}

usb_hcd_addr::~usb_hcd_addr() {
    hcd.ReleaseFuncAddr(addr);
}
