//
// Created by sigsegv on 9/10/21.
//

#include <klogger.h>
#include <thread>
#include "usb_port_connection.h"

usb_port_connection::usb_port_connection(usb_hub &hub, uint8_t port) : hub(hub), port(port) {
    get_klogger() << "USB port connected\n";
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }
    hub.EnablePort(port);
    {
        bool enabled{hub.EnabledPort(port)};
        int timeout = 50;
        while (timeout > 0 && !enabled) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            enabled = hub.EnabledPort(port);
            --timeout;
        }
        if (!enabled) {
            get_klogger() << "USB port failed to enable\n";
            return;
        }
    }
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
    }
    get_klogger() << "USB port enabled and reset\n";
    hub.DisablePort(port);
}

usb_port_connection::~usb_port_connection() {
    get_klogger() << "USB port disconnected\n";
}
