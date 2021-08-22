//
// Created by sigsegv on 8/18/21.
//

#include <sstream>
#include <chrono>
#include <thread>
#include "usb_hcd.h"

void usb_hcd::BusInit() {
    get_klogger() << "Disabling all ports on bus\n";
    for (int i = 0; i < GetNumberOfPorts(); i++) {
        uint32_t status{GetPortStatus(i)};
        {
            std::stringstream str{};
            str << "Switching off usb port with status (norm)" << std::hex << status << "\n";
            get_klogger() << str.str().c_str();
        }
        ClearStatusChange(i, status);
        SwitchPortOff(i);
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    get_klogger() << "Enabling connected ports on bus\n";
    for (int i = 0; i < GetNumberOfPorts(); i++) {
        uint32_t status{GetPortStatus(i)};
        ClearStatusChange(i, status);
        if ((status & USB_PORT_STATUS_CCS)) {
            {
                std::stringstream str{};
                str << "Switching on usb port with status (norm)" << std::hex << status << "\n";
                get_klogger() << str.str().c_str();
            }
            SwitchPortOn(i);
        } else {
            {
                std::stringstream str{};
                str << "Leaving usb port with status (norm)" << std::hex << status << "\n";
                get_klogger() << str.str().c_str();
            }
        }
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(200ms);
    }
    get_klogger() << "Enabled port statuses:\n";
    for (int i = 0; i < GetNumberOfPorts(); i++) {
        uint32_t status{GetPortStatus(i)};
        ClearStatusChange(i, status);
        if ((status & USB_PORT_STATUS_CCS)) {
            {
                std::stringstream str{};
                str << "Switched on usb port with status (norm)" << std::hex << status << "\n";
                get_klogger() << str.str().c_str();
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
