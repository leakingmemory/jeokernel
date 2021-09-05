//
// Created by sigsegv on 8/18/21.
//

#include <sstream>
#include <chrono>
#include <thread>
#include "usb_hcd.h"

void usb_hcd::BusInit() {
    get_klogger() << "Enabling all ports on bus\n";
    for (int i = 0; i < GetNumberOfPorts(); i++) {
        uint32_t status{GetPortStatus(i)};
        {
            std::stringstream str{};
            str << "Switching on usb port with status (norm)" << std::hex << status << "\n";
            get_klogger() << str.str().c_str();
        }
        ClearStatusChange(i, status);
        SwitchPortOn(i);
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    get_klogger() << "Port statuses:\n";
    for (int i = 0; i < GetNumberOfPorts(); i++) {
        uint32_t status{GetPortStatus(i)};
        ClearStatusChange(i, status);
        {
            std::stringstream str{};
            str << "Port status (norm)" << std::hex << status << "\n";
            get_klogger() << str.str().c_str();
        }
    }
}

void usb_hcd::Run() {
    std::thread thread{[this] () {
        this->BusInit();
    }};
    thread.detach();
}
