//
// Created by sigsegv on 10/30/21.
//

#ifndef JEOKERNEL_USBIFACEDEV_H
#define JEOKERNEL_USBIFACEDEV_H

#include <devices/drivers.h>
#include "usb_port_connection.h"

class usbifacedev : public Bus {
private:
    UsbDeviceInformation usbDeviceInformation;
public:
    usbifacedev(Bus &bus, const UsbDeviceInformation &usbDeviceInformation) :
        Bus("usbdev", &bus),
        usbDeviceInformation(usbDeviceInformation)
    { }
    void init() override;
};

class usbifacedev_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override {
        if (
                deviceInformation.device_class == 0 &&
                deviceInformation.device_subclass == 0 &&
                deviceInformation.prog_if == 0 &&
                deviceInformation.GetUsbInformation() != nullptr
           ) {
            return new usbifacedev(bus, *(deviceInformation.GetUsbInformation()));
        }
        return nullptr;
    }
};


#endif //JEOKERNEL_USBIFACEDEV_H
