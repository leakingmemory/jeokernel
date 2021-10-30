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

usb_port_connection::usb_port_connection(usb_hub &hub, uint8_t port) :
        hub(hub), port(port),
        addr(hub.GetFuncAddr()),
        thread(nullptr), endpoint0(),
        deviceDescriptor() {
    if (!addr) {
        get_klogger() << "Couldn't assign address to usb device\n";
    }
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
            addr = {};
            return;
        }
        bool enabled{hub.EnabledPort(port)};
        if (!enabled) {
            get_klogger() << "USB port failed to enable\n";
            addr = {};
            return;
        }
    }
    {
        std::stringstream str{};
        str << std::hex << "USB port enabled and reset - status " << hub.GetPortStatus(port) << "\n";
        get_klogger() << str.str().c_str();
    }
    usb_minimum_device_descriptor minDevDesc{};
    auto speed = hub.PortSpeed(port);
    {
        std::shared_ptr<usb_endpoint> ctrl_0_0 = hub.CreateControlEndpoint(8, 0, 0, usb_endpoint_direction::BOTH, speed);
        {
            usb_get_descriptor get_descr0{DESCRIPTOR_TYPE_DEVICE, 0, 8};
            std::shared_ptr<usb_buffer> descr0_buf = ControlRequest(*ctrl_0_0, get_descr0);
            if (descr0_buf) {
                descr0_buf->CopyTo(minDevDesc);
                {
                    std::stringstream str{};
                    str << std::hex << "Dev desc len=" << minDevDesc.bLength
                        << " type=" << minDevDesc.bDescriptorType << " USB=" << minDevDesc.bcdUSB
                        << " class=" << minDevDesc.bDeviceClass << " sub=" << minDevDesc.bDeviceSubClass
                        << " proto=" << minDevDesc.bDeviceProtocol << " packet-0=" << minDevDesc.bMaxPacketSize0
                        << "\n";
                    get_klogger() << str.str().c_str();
                }
            } else {
                get_klogger() << "Dev desc error\n";
                addr = {};
                hub.DisablePort(port);
                return;
            }
        }
        {
            uint8_t func = addr->GetAddr();
            usb_set_address set_addr{func};
            if (ControlRequest(*ctrl_0_0, set_addr)) {
                std::stringstream str{};
                str << std::hex << "Usb set address " << func << " success.\n";
                get_klogger() << str.str().c_str();
            } else {
                get_klogger() << "Usb set address failed\n";
                addr = {};
                hub.DisablePort(port);

                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(100ms);
                return;
            }
        }

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }

    thread = new std::thread([this, minDevDesc, speed] () {
        this->start(speed, minDevDesc);
    });
}

usb_port_connection::~usb_port_connection() {
    if (thread != nullptr) {
        thread->join();
        delete thread;
    }
    if (addr) {
        hub.DisablePort(port);
    }
    get_klogger() << "USB port disconnected\n";
}

std::shared_ptr<usb_buffer>
usb_port_connection::ControlRequest(usb_endpoint &endpoint, const usb_control_request &request) {
    std::shared_ptr<usb_transfer> t_req = endpoint.CreateTransfer(
                (void *) &request, sizeof(request),
                usb_transfer_direction::SETUP, true,
                1, 0
            );
    std::shared_ptr<usb_transfer> data_stage{};
    std::shared_ptr<usb_transfer> status_stage{};
    if (request.wLength > 0) {
        data_stage = endpoint.CreateTransfer(
                request.wLength,
                usb_transfer_direction::IN, true,
                1, 1);
        status_stage = endpoint.CreateTransfer(
                0,
                usb_transfer_direction::OUT, true,
                1, 1
                );
    } else {
        status_stage = endpoint.CreateTransfer(
                0,
                usb_transfer_direction::IN, true,
                1, 1
        );
    }
    timeout_wait(*t_req);
    if (!t_req->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed: " << t_req->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return {};
    }
    if (data_stage) {
        timeout_wait(*data_stage);
        if (!data_stage->IsSuccessful()) {
            std::stringstream str{};
            str << "Usb control transfer failed (data): " << data_stage->GetStatusStr() << "\n";
            get_klogger() << str.str().c_str();
            return {};
        }
    }
    timeout_wait(*status_stage);
    if (!status_stage->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed (status): " << status_stage->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return {};
    }
    if (data_stage) {
        return data_stage->Buffer();
    } else {
        return t_req->Buffer();
    }
}

void usb_port_connection::start(usb_speed speed, const usb_minimum_device_descriptor &minDesc) {
    uint8_t func = addr->GetAddr();
    {
        std::stringstream str{};
        str << std::hex << "Creating endpoint0 for addr " << func << "\n";
        get_klogger() << str.str().c_str();
    }
    endpoint0 = hub.CreateControlEndpoint(minDesc.bMaxPacketSize0, func, 0, usb_endpoint_direction::BOTH, speed);
    if (minDesc.bLength >= sizeof(deviceDescriptor)) {
        usb_get_descriptor get_descr0{DESCRIPTOR_TYPE_DEVICE, 0, sizeof(deviceDescriptor)};
        std::shared_ptr<usb_buffer> descr0_buf = ControlRequest(*endpoint0, get_descr0);
        if (descr0_buf) {
            descr0_buf->CopyTo(deviceDescriptor);
            {
                std::stringstream str{};
                str << std::hex << "Dev desc len=" << deviceDescriptor.bLength
                    << " type=" << deviceDescriptor.bDescriptorType << " USB=" << deviceDescriptor.bcdUSB
                    << " class=" << deviceDescriptor.bDeviceClass << " sub=" << deviceDescriptor.bDeviceSubClass
                    << " proto=" << deviceDescriptor.bDeviceProtocol << " packet-0=" << deviceDescriptor.bMaxPacketSize0
                    << "\n";
                str << std::hex << " id-vec=" << deviceDescriptor.idVector << " id-prod=" << deviceDescriptor.idProduct
                    << " dev-ver=" << deviceDescriptor.bcdDevice << " manu=" << deviceDescriptor.iManufacturer
                    << " prod=" << deviceDescriptor.iProduct << " ser=" << deviceDescriptor.iSerialNumber
                    << " num-conf=" << deviceDescriptor.bNumConfigurations << "\n";
                get_klogger() << str.str().c_str();
            }
        } else {
            get_klogger() << "Failed to get usb device descriptor for device\n";
        }
    }
}
