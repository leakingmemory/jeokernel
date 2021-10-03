//
// Created by sigsegv on 9/10/21.
//

#include <klogger.h>
#include <thread>
#include <sstream>
#include "usb_port_connection.h"

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
    {
        std::shared_ptr<usb_endpoint> ctrl_0_0 = hub.CreateControlEndpoint(8, 0, 0, BOTH, hub.PortSpeed(port));
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
