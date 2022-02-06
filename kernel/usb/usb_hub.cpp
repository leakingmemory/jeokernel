//
// Created by sigsegv on 1/31/22.
//

#include "usb_hub.h"
#include "thread"
#include "sstream"
#include "usb_port_connection.h"

void usb_hub::PortConnected(uint8_t port) {
    auto iterator = connections.begin();
    while (iterator != connections.end()) {
        auto *connection = *iterator;
        if (connection->Port() == port) {
            // Already connected
            if ((GetPortStatus(port) & USB_PORT_STATUS_PES) != 0) {
                return;
            } else {
                // Port disabled - assume disconnect+connect
                connections.erase(iterator);
                delete connection;
                break;
            }
        }
        ++iterator;
    }
    usb_port_connection *connection = new usb_port_connection(*this, port);
    connections.push_back(connection);
}

void usb_hub::PortDisconnected(uint8_t port) {
    auto iterator = connections.begin();
    while (iterator != connections.end()) {
        auto *connection = *iterator;
        if (connection->Port() == port) {
            connections.erase(iterator);
            connection->stop();
            delete connection;
            return;
        }
        ++iterator;
    }
}

bool usb_hub::InitHubPorts() {
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
    return true;
}

void usb_hub::RunPollPorts() {
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

usb_hub::~usb_hub() {
    for (auto *connection : connections) {
        connection->stop();
        delete connection;
    }
}

void usb_hub::stop() {
    for (auto *connection : connections) {
        connection->stop();
        delete connection;
    }
    connections.clear();
}
