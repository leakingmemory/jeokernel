//
// Created by sigsegv on 2/26/22.
//

#include "usb_port_connection.h"
#include "usbstorage.h"
#include <sstream>
#include "ustorage_structs.h"

#define IFACE_PROTO_BULK_ONLY 0x50

Device *usbstorage_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (deviceInformation.device_class == 0 && deviceInformation.device_subclass == 0 && deviceInformation.prog_if == 0) {
        auto *usbInfo = deviceInformation.GetUsbInformation();
        if (usbInfo != nullptr) {
            UsbIfacedevInformation *ifaceDev{nullptr};
            {
                UsbInterfaceInformation *ifaceInfo = usbInfo->GetUsbInterfaceInformation();
                if (ifaceInfo != nullptr) {
                    ifaceDev = ifaceInfo->GetIfaceDev();
                }
            }
            if (ifaceDev != nullptr && ifaceDev->iface.bInterfaceClass == 8 && ifaceDev->iface.bInterfaceProtocol == IFACE_PROTO_BULK_ONLY) {
                auto *dev = new usbstorage(&bus, *ifaceDev, ifaceDev->iface.bInterfaceSubClass);
                return dev;
            }
        }
    }
    return nullptr;
}

void usbstorage::init() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.SetConfigurationValue(devInfo.descr.bConfigurationValue, 0, 0)) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB storage set configuration failed\n";
        get_klogger() << str.str().c_str();
        return;
    }
    {
        int maxLun = GetMaxLun();
        if (maxLun < 0) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Error: USB storage get max lun failed\n";
            get_klogger() << str.str().c_str();
            return;
        }
        this->maxLun = maxLun;
    }

    usb_endpoint_descriptor bulkInDesc{};
    usb_endpoint_descriptor bulkOutDesc{};
    {
        bool bulkInFound{false};
        bool bulkOutFound{false};
        for (const auto &endpoint: devInfo.endpoints) {
            if (endpoint.IsBulk()) {
                if (endpoint.IsDirectionIn()) {
                    bulkInDesc = endpoint;
                    bulkInFound = true;
                } else {
                    bulkOutDesc = endpoint;
                    bulkOutFound = true;
                }
            }
        }
        if (!bulkInFound || !bulkOutFound) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Bulk endpoint, ";
            if (bulkInFound) {
                str << "out";
            } else if (bulkOutFound) {
                str << "in";
            } else {
                str << "both";
            }
            str << ", not found\n";
            get_klogger() << str.str().c_str();
            return;
        }
    }
    bulkInEndpoint = devInfo.port.BulkEndpoint(bulkInDesc.wMaxPacketSize, bulkInDesc.bEndpointAddress, usb_endpoint_direction::IN);
    bulkOutEndpoint = devInfo.port.BulkEndpoint(bulkOutDesc.wMaxPacketSize, bulkOutDesc.bEndpointAddress, usb_endpoint_direction::OUT);
    if (!bulkInEndpoint || !bulkOutEndpoint) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Failed to create bulk endpoint, ";
        if (bulkInEndpoint) {
            str << "out";
        } else if (bulkOutEndpoint) {
            str << "in";
        } else {
            str << "both";
        }
        str << "\n";
        get_klogger() << str.str().c_str();
        return;
    }

    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": subclass=" << subclass << " luns=" << (((int) maxLun) + 1) << "\n";
    get_klogger() << str.str().c_str();
}

void usbstorage::stop() {
}

usbstorage::~usbstorage() noexcept {
}

bool usbstorage::MassStorageReset() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.ControlRequest(*endpoint0, ustorage_mass_reset(devInfo.iface.bInterfaceNumber))) {
        return false;
    }
    return true;
}

int usbstorage::GetMaxLun() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    auto buffer = devInfo.port.ControlRequest(*endpoint0, ustorage_get_max_lun(devInfo.iface.bInterfaceNumber));
    if (!buffer) {
        return -1;
    }
    uint8_t maxLun;
    buffer->CopyTo(maxLun);
    return maxLun;
}