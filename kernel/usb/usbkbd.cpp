//
// Created by sigsegv on 10/31/21.
//

#include <sstream>
#include "usbkbd.h"
#include "usb_port_connection.h"
#include "usbifacedev.h"
#include "uhid_structs.h"

struct usbkbd_report {
    uint8_t modifierKeys;
    uint8_t reserved;
    uint8_t keypress[6];

    void dump() {
        std::stringstream str{};
        str << "keyboard m="<< std::hex << modifierKeys << " "
            << keypress[0] << " " << keypress[1] << " " << keypress[2] << " "
            << keypress[3] << " " << keypress[4] << " " << keypress[5] << "\n";
        get_klogger() << str.str().c_str();
    }
} __attribute__ ((__packed__));

Device *usbkbd_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    UsbInterfaceInformation *ifaceInfo{nullptr};
    {
        auto *usbInfo = deviceInformation.GetUsbInformation();
        if (usbInfo != nullptr) {
            ifaceInfo = usbInfo->GetUsbInterfaceInformation();
        }
    }
    if (ifaceInfo != nullptr) {
        auto ifaceDev = ifaceInfo->GetIfaceDev();
        if (
                ifaceInfo->iface.bInterfaceClass == 3 &&
                ifaceInfo->iface.bInterfaceSubClass == 1 &&
                ifaceInfo->iface.bInterfaceProtocol == 1 &&
                ifaceDev != nullptr) {
            auto *dev = new usbkbd(bus, *ifaceDev);
            ifaceDev->ifacedev.SetDevice(*dev);
            return dev;
        }
    }
    return nullptr;
}

void usbkbd::init() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.ControlRequest(*endpoint0, usb_set_configuration(devInfo.descr.bConfigurationValue))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set config failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    if (!devInfo.port.ControlRequest(*endpoint0, uhid_set_protocol(UHID_PROTO_BOOT))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set protocol failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    {
        std::shared_ptr<usb_buffer> report = devInfo.port.ControlRequest(*endpoint0, uhid_get_report());
        if (!report) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Error: USB keyboard test failed\n";
            get_klogger() << str.str().c_str();
            return;
        }
        usbkbd_report kbrep{};
        report->CopyTo(kbrep);
        kbrep.dump();
    }

    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": USB keyboard\n";
    get_klogger() << str.str().c_str();
}
