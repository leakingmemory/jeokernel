//
// Created by sigsegv on 10/30/21.
//

#include "usbifacedev.h"

void usbifacedev::init() {
    usbDeviceInformation.port.ReadConfigurations();
}

Device *usbifacedev_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    auto *usbInfo = deviceInformation.GetUsbInformation();
    if (
            deviceInformation.device_class == 0 &&
            deviceInformation.device_subclass == 0 &&
            deviceInformation.prog_if == 0 &&
            usbInfo != nullptr
            ) {
        auto *dev = new usbifacedev(bus, *(deviceInformation.GetUsbInformation()));
        usbInfo->port.SetDevice(*dev);
        return dev;
    }
    return nullptr;
}
