//
// Created by sigsegv on 9/10/21.
//

#include <klogger.h>
#include <thread>
#include <sstream>
#include "usb_port_connection.h"
#include "usb_control_req.h"

usb_port_connection::usb_port_connection(usb_hub &hub, uint8_t port) : hub(hub), port(port) {
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(200ms);
    }
    get_klogger() << "USB port reset\n";
    hub.ResetPort(port);
    {
        bool resetted{hub.EnabledPort(port)};
        int timeout = 50;
        while (timeout > 0 && !resetted) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            resetted = !hub.ResettingPort(port);
            --timeout;
        }
        if (!resetted) {
            get_klogger() << "USB port failed to reset\n";
            hub.DisablePort(port);
            return;
        }
        bool enabled{hub.EnabledPort(port)};
        if (!enabled) {
            get_klogger() << "USB port failed to enable\n";
            return;
        }
    }
    {
        std::stringstream str{};
        str << std::hex << "USB port enabled and reset - status " << hub.GetPortStatus(port) << "\n";
        get_klogger() << str.str().c_str();
    }
    auto speed = hub.PortSpeed(port);
    {
        std::shared_ptr<usb_endpoint> ctrl_0_0 = hub.CreateControlEndpoint(8, 0, 0, usb_endpoint_direction::BOTH, speed);
        usb_set_address set_addr{1}; // TODO - allocate and set addr
        std::shared_ptr<usb_transfer> t_setaddr = ctrl_0_0->CreateTransfer((void *) &set_addr, sizeof(set_addr), usb_transfer_direction::SETUP, false, 1);
        uint16_t packetSize{64};
        if (speed == LOW) {
            packetSize = 8;
        } else if (speed == FULL) {
            packetSize = 64;
        }
        std::shared_ptr<usb_transfer> t_setaddr_status = ctrl_0_0->CreateTransfer(packetSize, usb_transfer_direction::IN, false, 1, 1);
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    hub.DisablePort(port);
}

usb_port_connection::~usb_port_connection() {
    get_klogger() << "USB port disconnected\n";
}
