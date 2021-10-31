//
// Created by sigsegv on 10/31/21.
//

#include <sstream>
#include "usbkbd.h"
#include "usb_port_connection.h"
#include "usbifacedev.h"

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
    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": USB keyboard\n";
    get_klogger() << str.str().c_str();
}
