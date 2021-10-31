//
// Created by sigsegv on 9/10/21.
//

#include <klogger.h>
#include <thread>
#include <sstream>
#include <devices/drivers.h>
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
        deviceDescriptor(), device(nullptr),
        interfaces() {
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
    if (device != nullptr) {
        devices().remove(*device);
        delete device;
        device = nullptr;
    }
    if (thread != nullptr) {
        thread->join();
        delete thread;
    }
    if (addr) {
        hub.DisablePort(port);
    }
    get_klogger() << "USB port disconnected\n";
}

class long_usb_buffer : public usb_buffer {
private:
    void *buffer;
    size_t size;
public:
    long_usb_buffer(const std::vector<std::shared_ptr<usb_buffer>> &buffers) : buffer(nullptr), size{0} {
        for (const std::shared_ptr<usb_buffer> &buf : buffers) {
            size += buf->Size();
        }
        buffer = malloc(size);
        size_t offset{0};
        for (const std::shared_ptr<usb_buffer> &buf : buffers) {
            memcpy(((uint8_t *) buffer) + offset, buf->Pointer(), buf->Size());
            offset += buf->Size();
        }
    }
    ~long_usb_buffer() {
        if (buffer != nullptr) {
            free(buffer);
            buffer = nullptr;
        }
    }

    void *Pointer() override {
        return buffer;
    }
    uint64_t Addr() override {
        return 0;
    }
    size_t Size() override {
        return size;
    }
};

std::shared_ptr<usb_buffer>
usb_port_connection::ControlRequest(usb_endpoint &endpoint, const usb_control_request &request) {
    std::shared_ptr<usb_transfer> t_req = endpoint.CreateTransfer(
                (void *) &request, sizeof(request),
                usb_transfer_direction::SETUP, true,
                1, 0
            );
    std::vector<std::shared_ptr<usb_transfer>> data_stage{};
    std::shared_ptr<usb_transfer> status_stage{};
    if (request.wLength > 0) {
        auto remaining = request.wLength;
        while (remaining > 0) {
            auto data_stage_tr = endpoint.CreateTransfer(
                    request.wLength,
                    usb_transfer_direction::IN, true,
                    1, 1);
            data_stage.push_back(data_stage_tr);
            auto size = data_stage_tr->Buffer()->Size();
            if (size < remaining) {
                remaining -= size;
            } else {
                remaining = 0;
            }
        }
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
    if (!data_stage.empty()) {
        for (const auto &stage : data_stage) {
            timeout_wait(*stage);
            if (!stage->IsSuccessful()) {
                std::stringstream str{};
                str << "Usb control transfer failed (data): " << stage->GetStatusStr() << "\n";
                get_klogger() << str.str().c_str();
                return {};
            }
        }
    }
    timeout_wait(*status_stage);
    if (!status_stage->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed (status): " << status_stage->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return {};
    }
    if (!data_stage.empty()) {
        std::vector<std::shared_ptr<usb_buffer>> buffers{};
        for (const auto &stage : data_stage) {
            buffers.push_back(stage->Buffer());
        }
        return std::shared_ptr(new long_usb_buffer(buffers));
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
            UsbDeviceInformation devInfo{deviceDescriptor, *this};
            get_drivers().probe(hub, devInfo);
        } else {
            get_klogger() << "Failed to get usb device descriptor for device\n";
        }
    }
}

const std::vector<UsbInterfaceInformation> &usb_port_connection::ReadConfigurations(const UsbDeviceInformation &devInfo) {
    if (!interfaces.empty()) {
        return interfaces;
    }
    for (int index = 0; index < deviceDescriptor.bNumConfigurations; index++) {
        usb_configuration_descriptor config{};
        std::shared_ptr<usb_buffer> short_buffer = ControlRequest(*endpoint0,
                                                                  usb_get_descriptor(DESCRIPTOR_TYPE_CONFIGURATION, index,
                                                                  sizeof(config)));
        if (short_buffer) {
            short_buffer->CopyTo(config);
            {
                std::stringstream str{};
                str << std::hex << "conf len=" << config.bLength << " type=" << config.bDescriptorType
                    << " tot-len=" << config.wTotalLength << " num-iface=" << config.bNumInterfaces
                    << " conf-val=" << config.bConfigurationValue << " i-conf=" << config.iConfiguration
                    << " attr=" << config.bmAttributes << " max-pow=" << config.bMaxPower << "\n";
                get_klogger() << str.str().c_str();
            }
            std::shared_ptr<usb_buffer> buffer = ControlRequest(*endpoint0,
                                                                usb_get_descriptor(DESCRIPTOR_TYPE_CONFIGURATION, index,
                                                                config.wTotalLength));
            if (buffer) {
                size_t offset{0};
                offset += config.bLength;
                int c_interfaces{0};
                while ((c_interfaces++) < config.bNumInterfaces) {
                    usb_interface_descriptor iface{};
                    if (offset > buffer->Size() || (buffer->Size() - offset) < sizeof(iface)) {
                        get_klogger() << "Buffer underrun with attachments to config descriptor\n";
                        break;
                    }
                    buffer->CopyTo(iface, offset);
                    offset += iface.bLength;

                    {
                        std::stringstream str{};
                        str << std::hex << "Iface " << iface.bLength << " " << iface.bDescriptorType
                            << " " << iface.bInterfaceNumber << " alt=" << iface.bAlternateSetting
                            << " endp=" << iface.bNumEndpoints << " class=" << iface.bInterfaceClass
                            << "/" << iface.bInterfaceSubClass << "/" << iface.bInterfaceProtocol
                            << " str=" << iface.iInterface << "\n";
                        get_klogger() << str.str().c_str();
                    }

                    UsbInterfaceInformation ifaceInfo{devInfo, config, iface};

                    int c_endpoints{0};
                    while ((c_endpoints++) < iface.bNumEndpoints) {
                        usb_endpoint_descriptor endpoint{};
                        if (offset > buffer->Size() || (buffer->Size() - offset) < sizeof(endpoint)) {
                            get_klogger() << "Buffer underrun with attachments to config descriptor\n";
                            break;
                        }
                        buffer->CopyTo(endpoint, offset);
                        offset += endpoint.bLength;

                        if (endpoint.bDescriptorType != 5) {
                            --c_endpoints;
                            continue;
                        }

                        {
                            std::stringstream str{};
                            str << std::hex << "endp " << endpoint.bLength << " " << endpoint.bDescriptorType
                                << " addr=" << endpoint.bEndpointAddress << " att=" << endpoint.bmAttributes
                                << " max-pack=" << endpoint.wMaxPacketSize << " interv=" << endpoint.bInterval << "\n";
                            get_klogger() << str.str().c_str();
                        }

                        ifaceInfo.endpoints.push_back(endpoint);
                    }

                    interfaces.push_back(ifaceInfo);
                }
            } else {
                get_klogger() << "Failed to read attachments to config descriptor\n";
            }
        } else {
            get_klogger() << "Failed to get config descriptor\n";
        }
    }
    return interfaces;
}

UsbDeviceInformation::UsbDeviceInformation(const usb_device_descriptor &devDesc, usb_port_connection &port) : DeviceInformation(), port(port) {
    vendor_id = devDesc.idVector;
    device_id = devDesc.idProduct;
    device_subclass = devDesc.bDeviceSubClass;
    device_class = devDesc.bDeviceClass;
    prog_if = devDesc.bDeviceProtocol;
}

UsbDeviceInformation::UsbDeviceInformation(const UsbDeviceInformation &cp) : port(cp.port) {
    vendor_id = cp.vendor_id;
    device_id = cp.device_id;
    device_subclass = cp.device_subclass;
    device_class = cp.device_class;
    prog_if = cp.prog_if;
}

UsbDeviceInformation *UsbDeviceInformation::GetUsbInformation() {
    return this;
}

UsbInterfaceInformation *UsbDeviceInformation::GetUsbInterfaceInformation() {
    return nullptr;
}

UsbInterfaceInformation::UsbInterfaceInformation(
        const UsbDeviceInformation &devInfo,
        const usb_configuration_descriptor &descr,
        const usb_interface_descriptor &iface)
        : UsbDeviceInformation(devInfo),
          descr(descr),
          iface(iface),
          endpoints() {
}

UsbInterfaceInformation::UsbInterfaceInformation(const UsbInterfaceInformation &cp)
: UsbDeviceInformation(cp),
  descr(cp.descr),
  iface(cp.iface),
  endpoints(cp.endpoints) {
}

UsbInterfaceInformation *UsbInterfaceInformation::GetUsbInterfaceInformation() {
    return this;
}

UsbIfacedevInformation *UsbInterfaceInformation::GetIfaceDev() {
    return nullptr;
}
