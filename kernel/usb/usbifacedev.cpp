//
// Created by sigsegv on 10/30/21.
//

#include "usbifacedev.h"

UsbIfacedevInformation::UsbIfacedevInformation(usbifacedev &ifacedev, const UsbInterfaceInformation &info)
: UsbInterfaceInformation(info), ifacedev(ifacedev) {

}

UsbIfacedevInformation *UsbIfacedevInformation::GetIfaceDev() {
    return this;
}

UsbIfacedevInformation::UsbIfacedevInformation(const UsbIfacedevInformation &cp)
: UsbInterfaceInformation(cp), ifacedev(cp.ifacedev) {
}


void usbifacedev::init() {
    const auto &interfaces = usbDeviceInformation.port.ReadConfigurations(usbDeviceInformation);
    for (auto &interface : interfaces) {
        UsbIfacedevInformation iface{*this, interface};
        get_drivers().probe(*this, iface);
    }
}

void usbifacedev::stop() {
    if (device != nullptr) {
        devices().remove(*device);
        delete device;
        device = nullptr;
    }
}

usbifacedev::~usbifacedev() {
    if (device != nullptr) {
        devices().remove(*device);
        delete device;
        device = nullptr;
    }
}

Device *usbifacedev_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    auto *usbInfo = deviceInformation.GetUsbInformation();
    bool interfaceDevice = deviceInformation.device_class == 0 &&
                           deviceInformation.device_subclass == 0 &&
                           deviceInformation.prog_if == 0;
    if (
            usbInfo != nullptr &&
            usbInfo->GetUsbInterfaceInformation() == nullptr &&
            (interfaceDevice || deviceInformation.device_class == 9/*hub*/)
            ) {
        auto *dev = new usbifacedev(bus, *(deviceInformation.GetUsbInformation()));
        usbInfo->port.SetDevice(*dev);
        return dev;
    }
    return nullptr;
}
