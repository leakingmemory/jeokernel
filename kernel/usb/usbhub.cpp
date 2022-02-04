//
// Created by sigsegv on 1/30/22.
//

#include "usbhub.h"
#include "sstream"

#define HUB_PORT_POWER_MASK         3
#define HUB_PORT_POWER_GLOBAL       0
#define HUB_PORT_POWER_INDIVIDUAL   1

Device *usbhub_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (deviceInformation.device_class == 9) {
        auto *usbInfo = deviceInformation.GetUsbInformation();
        if (usbInfo != nullptr) {
            UsbInterfaceInformation *ifaceInfo = usbInfo->GetUsbInterfaceInformation();
            if (ifaceInfo != nullptr && ifaceInfo->iface.bInterfaceClass == 9) {
                auto *dev = new usbhub(bus, *ifaceInfo);
                usbInfo->port.SetDevice(*dev);
                return dev;
            }
        }
    }
    return nullptr;
}

usbhub::~usbhub() {
    if (ready) {
        usbInterfaceInformation.port.Hub().UnregisterHub(this);
    }
}

void usbhub::init() {
    endpoint0 = usbInterfaceInformation.port.Endpoint0();

    if (!usbInterfaceInformation.port.SetConfigurationValue(usbInterfaceInformation.descr.bConfigurationValue, 0, 0)) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB hub set config failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    {
        auto buffer = usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_hubdesc(sizeof(descr)));
        if (!buffer) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": failed to get hub descriptor\n";
            get_klogger() << str.str().c_str();
            return;
        }
        buffer->CopyTo(descr);
    }
    individualPortPower = (descr.flags & HUB_PORT_POWER_MASK) == HUB_PORT_POWER_INDIVIDUAL;
    {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": ports=" << descr.portCount << " type=" << descr.type << " flags=" << descr.flags << " port-time=" << descr.portPowerTime << " current=" << descr.current << (individualPortPower ? " individual port power" : " global port power") << "\n";
        get_klogger() << str.str().c_str();
    }
    usbInterfaceInformation.port.Hub().RegisterHub(this);
    {
        std::lock_guard lock{ready_mtx};
        ready = true;
    }
}

void usbhub::dumpregs() {
}

int usbhub::GetNumberOfPorts() {
    return descr.portCount;
}

uint32_t usbhub::GetPortStatus(int port) {
    uint32_t hwstatus{0};
    {
        auto buffer = usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_hubportstatus(port));
        if (!buffer) {
            return 0;
        }
        buffer->CopyTo(hwstatus);
    }
    uint32_t status{0};
    if ((hwstatus & 1) != 0) {
        status |= USB_PORT_STATUS_CCS;
    }
    if ((hwstatus & 2) != 0) {
        status |= USB_PORT_STATUS_PES;
    }
    if ((hwstatus & 4) != 0) {
        status |= USB_PORT_STATUS_PSS;
    }
    if ((hwstatus & 8) != 0) {
        status |= USB_PORT_STATUS_POCI;
    }
    if ((hwstatus & 16) != 0) {
        status |= USB_PORT_STATUS_PRS;
    }
    if ((hwstatus & (1 << 8)) != 0) {
        status |= USB_PORT_STATUS_PPS;
    }
    uint32_t speed{(hwstatus >> 9) & 3};
    if ((hwstatus & (1 << 16)) != 0) {
        status |= USB_PORT_STATUS_CSC;
    }
    if ((hwstatus & (1 << 17)) != 0) {
        status |= USB_PORT_STATUS_PESC;
    }
    if ((hwstatus & (1 << 18)) != 0) {
        status |= USB_PORT_STATUS_OCIC;
    }
    switch (usbInterfaceInformation.port.Speed()) {
        case LOW:
            status |= USB_PORT_STATUS_LSDA;
            break;
        default:
            if (speed == 1) {
                status |= USB_PORT_STATUS_LSDA;
            }
    }
    return status;
}

void usbhub::SwitchPortOff(int port) {
    if (individualPortPower) {
        if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::POWER))) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed port power off request\n";
            get_klogger() << str.str().c_str();
        }
    }
}

void usbhub::SwitchPortOn(int port) {
    if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_set_port_feature(port, usb_hub_port_feature::POWER))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Failed port power request\n";
        get_klogger() << str.str().c_str();
    }
}

void usbhub::ClearStatusChange(int port, uint32_t statuses) {
    if ((statuses & USB_PORT_STATUS_CSC) != 0) {
        if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::CONNECTION_CHANGE))) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed port connection change clear\n";
            get_klogger() << str.str().c_str();
        }
    }
    if ((statuses & USB_PORT_STATUS_PESC) != 0) {
        if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::ENABLE_CHANGE))) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed port enable change clear\n";
            get_klogger() << str.str().c_str();
        }
    }
    if ((statuses & USB_PORT_STATUS_OCIC) != 0) {
        if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::OVERCURRENT_CHANGE))) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed port overcurrent change clear\n";
            get_klogger() << str.str().c_str();
        }
    }
    if ((statuses & USB_PORT_STATUS_PSSC) != 0) {
        if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::SUSPEND_CHANGE))) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed port suspend change clear\n";
            get_klogger() << str.str().c_str();
        }
    }
    if ((statuses & USB_PORT_STATUS_PRSC) != 0) {
        if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::RESET_CHANGE))) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed port reset change clear\n";
            get_klogger() << str.str().c_str();
        }
    }
}

void usbhub::EnablePort(int port) {
}

void usbhub::DisablePort(int port) {
    if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_clear_port_feature(port, usb_hub_port_feature::ENABLE))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Failed port disable request\n";
        get_klogger() << str.str().c_str();
    }
}

void usbhub::ResetPort(int port) {
    if (!usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_set_port_feature(port, usb_hub_port_feature::RESET))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Failed port disable request\n";
        get_klogger() << str.str().c_str();
    }
}

bool usbhub::ResettingPort(int port) {
    return (GetPortStatus(port) & USB_PORT_STATUS_PRS) != 0;
}

bool usbhub::EnabledPort(int port) {
    return (GetPortStatus(port) & USB_PORT_STATUS_PES) != 0;
}

usb_speed usbhub::PortSpeed(int port) {
    uint32_t hwstatus{0};
    {
        auto buffer = usbInterfaceInformation.port.ControlRequest(*endpoint0, usb_req_hubportstatus(port));
        if (!buffer) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Failed to read port speed\n";
            get_klogger() << str.str().c_str();
            return LOW;
        }
        buffer->CopyTo(hwstatus);
    }
    uint32_t speed{(hwstatus >> 9) & 3};
    auto hubSpeed = usbInterfaceInformation.port.Speed();
    switch (hubSpeed) {
        case HIGH:
            if ((speed & 2) != 0) {
                return HIGH;
            } else if ((speed & 1) != 0) {
                return LOW;
            } else {
                return FULL;
            }
        case FULL:
            if ((speed & 1) != 0) {
                return LOW;
            } else {
                return FULL;
            }
        case LOW:
            return LOW;
        case SUPER:
            switch (speed) {
                case 0:
                    return FULL;
                case 1:
                    return LOW;
                case 2:
                    return HIGH;
                default:
                    return SUPER;
            }
        default:
            return hubSpeed;
    }
}

std::shared_ptr<usb_endpoint>
usbhub::CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                              uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    return usbInterfaceInformation.port.Hub().CreateControlEndpoint(
            portRouting, hubAddress,
            maxPacketSize, functionAddr, endpointNum,
            dir, speed
    );
}

std::shared_ptr<usb_endpoint>
usbhub::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                                uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed,
                                int pollingIntervalMs) {
    return usbInterfaceInformation.port.Hub().CreateInterruptEndpoint(
            portRouting, hubAddress,
            maxPacketSize, functionAddr, endpointNum,
            dir, speed, pollingIntervalMs
    );
}

std::shared_ptr<usb_func_addr> usbhub::GetFuncAddr() {
    return usbInterfaceInformation.port.Hub().GetFuncAddr();
}

void usbhub::RegisterHub(usb_hub *child) {
    usbInterfaceInformation.port.Hub().RegisterHub(child);
}

void usbhub::UnregisterHub(usb_hub *child) {
    usbInterfaceInformation.port.Hub().UnregisterHub(child);
}

void usbhub::PortRouting(std::vector<uint8_t> &route, uint8_t port) {
    usbInterfaceInformation.port.Hub().PortRouting(route, usbInterfaceInformation.port.Port());
    route.push_back(port);
}

uint8_t usbhub::GetHubAddress() {
    return usbInterfaceInformation.port.Address();
}

std::shared_ptr<usb_hw_enumeration_addressing> usbhub::EnumerateHubPort(const std::vector<uint8_t> &portRouting) {
    return usbInterfaceInformation.port.Hub().EnumerateHubPort(portRouting);
}
