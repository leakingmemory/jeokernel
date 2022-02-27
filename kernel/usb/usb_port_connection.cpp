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

class legacy_usb_enumerated : public usb_hw_enumerated_device, public control_request_trait {
private:
    usb_hub &hub;
    usb_speed speed;
    usb_minimum_device_descriptor minDesc;
    std::shared_ptr<usb_endpoint> endpoint0;
    std::vector<uint8_t> portRouting;
    uint8_t addr;
public:
    legacy_usb_enumerated(usb_hub &hub, uint8_t port, uint8_t func, usb_speed speed, usb_minimum_device_descriptor minDesc) : hub(hub), speed(speed), minDesc(minDesc), endpoint0(), portRouting(), addr(func) {
        {
            std::stringstream str{};
            str << std::hex << "Creating endpoint0 for addr " << func << "\n";
            get_klogger() << str.str().c_str();
        }
        hub.PortRouting(portRouting, port);
        endpoint0 = hub.CreateControlEndpoint(portRouting, hub.GetHubAddress(), minDesc.bMaxPacketSize0, func, 0, usb_endpoint_direction::BOTH, speed);
    }
    void Stop() override;
    usb_speed Speed() const override;
    uint8_t SlotId() const override;
    usb_minimum_device_descriptor MinDesc() const override;
    std::shared_ptr<usb_endpoint> Endpoint0() const override;
    bool SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime) override;
    bool SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir, int pollingIntervalMs) override;
    std::shared_ptr<usb_endpoint> CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir) override;
};

void legacy_usb_enumerated::Stop() {
}

usb_speed legacy_usb_enumerated::Speed() const {
    return speed;
}

uint8_t legacy_usb_enumerated::SlotId() const {
    return 0;
}

usb_minimum_device_descriptor legacy_usb_enumerated::MinDesc() const {
    return minDesc;
}

std::shared_ptr<usb_endpoint> legacy_usb_enumerated::Endpoint0() const {
    return endpoint0;
}

bool legacy_usb_enumerated::SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime) {
    return true;
}

bool legacy_usb_enumerated::SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting) {

    if (ControlRequest(*endpoint0, usb_set_configuration(configurationValue))) {
        return true;
    } else {
        return false;
    }
}

std::shared_ptr<usb_endpoint>
legacy_usb_enumerated::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress,
                                               uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir,
                                               int pollingIntervalMs) {
    return hub.CreateInterruptEndpoint(portRouting, hubAddress, maxPacketSize, addr, endpointNum, dir, speed, pollingIntervalMs);
}

std::shared_ptr<usb_endpoint>
legacy_usb_enumerated::CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress,
                                          uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir) {
    return hub.CreateBulkEndpoint(portRouting, hubAddress, maxPacketSize, addr, endpointNum, dir, speed);
}

usb_port_connection::usb_port_connection(usb_hub &hub, uint8_t port) :
        hub(hub), port(port),
        addr(hub.GetFuncAddr()), speed(LOW),
        thread(nullptr), endpoint0(), enumeratedDevice(),
        deviceDescriptor(), device(nullptr),
        interfaces(), portRouting() {
    if (!addr) {
        get_klogger() << "Couldn't assign address to usb device\n";
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(200ms);
    }

    usb_minimum_device_descriptor minDevDesc{};

    auto hc_enumeration = hub.EnumeratePort(port);

    if (hc_enumeration) {
        auto addressing = hc_enumeration->enumerate();
        if (!addressing) {
            get_klogger() << "USB port failed to enumerate\n";
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(100ms);
            if (hub.EnabledPort(port)) {
                hub.DisablePort(port);
            }
            return;
        }
        auto usbDevice = addressing->set_address(addr->GetAddr());
        if (!usbDevice) {
            get_klogger() << "USB port failed to set address\n";
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(100ms);
            if (hub.EnabledPort(port)) {
                hub.DisablePort(port);
            }
            return;
        }
        final_addr = addressing->get_address();
        speed = hub.PortSpeed(port);
        hub.PortRouting(portRouting, port);
        get_klogger() << "USB port configured\n";
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(100ms);

        thread = new std::thread([this, usbDevice] () {
            std::this_thread::set_name("[usbconn]");
            this->start(usbDevice);
        });
        return;
    } else {
        get_klogger() << "USB port reset\n";
        hub.ResetPort(port);
        {
            bool resetted{hub.EnabledPort(port)};
            int timeout = 50;
            while (timeout > 0 && !resetted) {
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(10ms);
                resetted = !hub.ResettingPort(port);
                if (resetted && !hub.PortResetEnablesPort()) {
                    resetted = hub.EnabledPort(port);
                    if (!resetted) {
                        hub.EnablePort(port);
                    }
                }
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
        speed = hub.PortSpeed(port);
        hub.PortRouting(portRouting, port);
        auto hw_enumerate_hub = hub.EnumerateHubPort(portRouting, speed, hub.HubSpeed(), hub.HubSlotId());
        if (hw_enumerate_hub) {
            auto usbDevice = hw_enumerate_hub->set_address(addr->GetAddr());
            if (!usbDevice) {
                get_klogger() << "USB port failed to set address\n";
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(100ms);
                if (hub.EnabledPort(port)) {
                    hub.DisablePort(port);
                }
                return;
            }
            final_addr = hw_enumerate_hub->get_address();
            speed = hub.PortSpeed(port);
            hub.PortRouting(portRouting, port);
            get_klogger() << "USB port configured\n";
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(100ms);

            thread = new std::thread([this, usbDevice] () {
                std::this_thread::set_name("[usbconn]");
                this->start(usbDevice);
            });
            return;
        } else {
            std::shared_ptr<usb_endpoint> ctrl_0_0 = hub.CreateControlEndpoint(portRouting, hub.GetHubAddress(),
                                                                               8, 0, 0,
                                                                               usb_endpoint_direction::BOTH, speed);
            if (ctrl_0_0) {
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
            } else {
                get_klogger() << "Usb connection: Can't create control endpoints\n";
                addr = {};
                hub.DisablePort(port);
                return;
            }
            {
                uint8_t func = addr->GetAddr();
                usb_set_address set_addr{func};
                if (ControlRequest(*ctrl_0_0, set_addr)) {
                    final_addr = func;
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
    }

    thread = new std::thread([this, minDevDesc] () {
        std::shared_ptr<usb_hw_enumerated_device> enumeratedDevice{
            new legacy_usb_enumerated(this->hub, this->port, addr->GetAddr(), speed, minDevDesc)
        };
        this->start(enumeratedDevice);
    });
}

usb_port_connection::~usb_port_connection() {
    enumeratedDevice = nullptr;
    endpoint0 = nullptr;
    get_klogger() << "USB port disconnected\n";
}

void usb_port_connection::stop() {
    if (enumeratedDevice) {
        enumeratedDevice->Stop();
    }
    if (device != nullptr) {
        devices().remove(*device);
        delete device;
        device = nullptr;
    }
    if (thread != nullptr) {
        thread->join();
        delete thread;
    }
    enumeratedDevice = nullptr;
    if (addr) {
        hub.DisablePort(port);
    }
    get_klogger() << "USB port disabled\n";
}

std::shared_ptr<usb_endpoint> usb_port_connection::InterruptEndpoint(int maxPacketSize, uint8_t endpointNum, usb_endpoint_direction direction, int pollingIntervalMs) {
    return enumeratedDevice->CreateInterruptEndpoint(portRouting, hub.GetHubAddress(), maxPacketSize, endpointNum, direction, pollingIntervalMs);
}

std::shared_ptr<usb_endpoint>
usb_port_connection::BulkEndpoint(int maxPacketSize, uint8_t endpointNum, usb_endpoint_direction direction) {
    return enumeratedDevice->CreateBulkEndpoint(portRouting, hub.GetHubAddress(), maxPacketSize, endpointNum, direction);
}

void usb_port_connection::start(std::shared_ptr<usb_hw_enumerated_device> enumeratedDevice) {
    usb_speed speed{enumeratedDevice->Speed()};
    usb_minimum_device_descriptor minDesc{enumeratedDevice->MinDesc()};
    this->enumeratedDevice = enumeratedDevice;
    endpoint0 = enumeratedDevice->Endpoint0();
    if (!endpoint0) {
        get_klogger() << "Failed to get endpoint0 for device\n";
        return;
    }
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

bool usb_port_connection::SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting) {
    return enumeratedDevice->SetConfigurationValue(configurationValue, interfaceNumber, alternateSetting);
}

bool usb_port_connection::SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime) {
    return enumeratedDevice->SetHub(numberOfPorts, multiTT, ttThinkTime);
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
