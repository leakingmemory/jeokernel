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
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_USBIFACEDEV_H
