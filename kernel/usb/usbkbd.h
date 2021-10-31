//
// Created by sigsegv on 10/31/21.
//

#ifndef JEOKERNEL_USBKBD_H
#define JEOKERNEL_USBKBD_H


#include <devices/devices.h>
#include <devices/drivers.h>
#include "usbifacedev.h"

class usbkbd : public Device {
private:
    UsbIfacedevInformation devInfo;
public:
    usbkbd(Bus &bus, UsbIfacedevInformation &devInfo) : Device("usbkbd", &bus), devInfo(devInfo) {
    }
    void init() override;
};

class usbkbd_driver : public Driver {
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_USBKBD_H
