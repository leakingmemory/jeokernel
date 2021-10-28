//
// Created by sigsegv on 9/10/21.
//

#include <klogger.h>
#include <thread>
#include <sstream>
#include "usb_port_connection.h"
#include "usb_control_req.h"
#include "usb_transfer.h"

static void timeout_wait(const usb_transfer &transfer) {
    int timeout = 25;
    while (!transfer.IsDone()) {
        if (--timeout == 0)
            break;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(40ms);
    }
}

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
        {
            usb_get_descriptor get_descr0{DESCRIPTOR_TYPE_DEVICE, 0, 8};
            std::shared_ptr<usb_transfer> t_get_descr0 = ctrl_0_0->CreateTransfer((void *) &get_descr0,
                                                                                  sizeof(get_descr0),
                                                                                  usb_transfer_direction::SETUP, true,
                                                                                  1, 0);
            std::shared_ptr<usb_transfer> t_get_descr0_result = ctrl_0_0->CreateTransfer(8, usb_transfer_direction::IN,
                                                                                         true, 1, 1);
            std::shared_ptr<usb_transfer> t_get_descr0_status = ctrl_0_0->CreateTransfer(0, usb_transfer_direction::OUT,
                                                                                         true, 1);
            timeout_wait(*t_get_descr0);
            if (t_get_descr0->IsSuccessful()) {
                timeout_wait(*t_get_descr0_result);
                if (t_get_descr0_result->IsSuccessful()) {
                    usb_minimum_device_descriptor minDevDesc{};
                    t_get_descr0_result->Buffer()->CopyTo(minDevDesc);
                    {
                        std::stringstream str{};
                        str << std::hex << "Dev desc len=" << minDevDesc.bLength
                            << " type=" << minDevDesc.bDescriptorType << " USB=" << minDevDesc.bcdUSB
                            << " class=" << minDevDesc.bDeviceClass << " sub=" << minDevDesc.bDeviceSubClass
                            << " proto=" << minDevDesc.bDeviceProtocol << " packet-0=" << minDevDesc.bMaxPacketSize0
                            << "\n";
                        get_klogger() << str.str().c_str();
                    }
                    timeout_wait(*t_get_descr0_status);
                    if (t_get_descr0_status->IsSuccessful()) {
                        std::stringstream str{};
                        str << "Usb get descr successful\n";
                        get_klogger() << str.str().c_str();
                    } else {
                        std::stringstream str{};
                        str << "Usb get descr (status): " << t_get_descr0_status->GetStatusStr() << "\n";
                        get_klogger() << str.str().c_str();
                    }
                } else {
                    std::stringstream str{};
                    str << "Usb get descr (result): " << t_get_descr0_result->GetStatusStr() << "\n";
                    get_klogger() << str.str().c_str();
                }
            } else {
                std::stringstream str{};
                str << "Usb get descr: " << t_get_descr0->GetStatusStr() << "\n";
                get_klogger() << str.str().c_str();
            }
        }
        {
            usb_set_address set_addr{1}; // TODO - allocate and set addr
            std::shared_ptr<usb_transfer> t_setaddr = ctrl_0_0->CreateTransfer((void *) &set_addr, sizeof(set_addr),
                                                                               usb_transfer_direction::SETUP, false, 1, 0);
            std::shared_ptr<usb_transfer> t_setaddr_status = ctrl_0_0->CreateTransfer(0,
                                                                                      usb_transfer_direction::IN, true,
                                                                                      1, 1);

            timeout_wait(*t_setaddr);
            if (t_setaddr->IsSuccessful()) {
                timeout_wait(*t_setaddr_status);
                if (t_setaddr_status->IsSuccessful()) {
                    get_klogger() << "Usb set address success\n";
                } else {
                    std::stringstream str{};
                    str << "Usb set address (status) failed: " << t_setaddr_status->GetStatusStr() << "\n";
                    get_klogger() << str.str().c_str();
                }
            } else {
                std::stringstream str{};
                str << "Usb set address failed: " << t_setaddr->GetStatusStr() << "\n";
                get_klogger() << str.str().c_str();
            }
        }

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    hub.DisablePort(port);
}

usb_port_connection::~usb_port_connection() {
    get_klogger() << "USB port disconnected\n";
}
