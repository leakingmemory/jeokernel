//
// Created by sigsegv on 8/18/21.
//

#include <sstream>
#include <chrono>
#include <thread>
#include "usb_hcd.h"

[[noreturn]] void usb_hcd::BusInit() {
    get_klogger() << "Enabling all ports on bus\n";
    for (int i = 0; i < GetNumberOfPorts(); i++) {
        uint32_t status{GetPortStatus(i)};
        if ((status & USB_PORT_STATUS_PPS) == 0) {
            {
                std::stringstream str{};
                str << "Switching on usb port with status (norm)" << std::hex << status << "\n";
                get_klogger() << str.str().c_str();
            }
            SwitchPortOn(i);
        }
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    while (true) {
        hub_sema.acquire();
        for (int i = 0; i < GetNumberOfPorts(); i++) {
            uint32_t status{GetPortStatus(i)};
            if ((status & USB_PORT_STATUS_CSC) != 0) {
                ClearStatusChange(i, USB_PORT_STATUS_CSC);
                if ((status & USB_PORT_STATUS_CCS) != 0) {
                    PortConnected(i);
                } else {
                    PortDisconnected(i);
                }
            }
        }
    }
}

void usb_hcd::Run() {
    std::thread thread{[this] () {
        this->BusInit();
    }};
    thread.detach();
}

void usb_hcd::RootHubStatusChange() {
    hub_sema.release();
}

void usb_hcd::PortConnected(uint8_t port) {
    for (auto *connection : connections) {
        if (connection->Port() == port) {
            // Already connected
            return;
        }
    }
    usb_port_connection *connection = new usb_port_connection(port);
    connections.push_back(connection);
}

void usb_hcd::PortDisconnected(uint8_t port) {
    auto iterator = connections.begin();
    while (iterator != connections.end()) {
        auto *connection = *iterator;
        if (connection->Port() == port) {
            connections.erase(iterator);
            delete connection;
            return;
        }
        ++iterator;
    }
}
