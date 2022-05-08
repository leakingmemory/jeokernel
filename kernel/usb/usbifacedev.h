//
// Created by sigsegv on 10/30/21.
//

#ifndef JEOKERNEL_USBIFACEDEV_H
#define JEOKERNEL_USBIFACEDEV_H

#include <devices/drivers.h>
#include "usb_port_connection.h"

class usbifacedev;

struct UsbIfacedevInformation : public UsbInterfaceInformation {
    usbifacedev &ifacedev;

    UsbIfacedevInformation(usbifacedev &ifacedev, const UsbInterfaceInformation &info);
    UsbIfacedevInformation(const UsbIfacedevInformation &cp);
    UsbIfacedevInformation *GetIfaceDev() override;
};

class usbifacedev : public Bus {
private:
    UsbDeviceInformation usbDeviceInformation;
    Device *device;
public:
    usbifacedev(Bus &bus, const UsbDeviceInformation &usbDeviceInformation) :
        Bus("usbdev", &bus),
        usbDeviceInformation(usbDeviceInformation),
        device(nullptr)
    { }
    ~usbifacedev() override;
    void init() override;
    void stop() override;
    void SetDevice(Device &dev) {
        device = &dev;
    }
};

class usbifacedev_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_USBIFACEDEV_H
