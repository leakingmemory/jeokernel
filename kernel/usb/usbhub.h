//
// Created by sigsegv on 1/30/22.
//

#ifndef JEOKERNEL_USBHUB_H
#define JEOKERNEL_USBHUB_H

#include <devices/drivers.h>
#include "usb_port_connection.h"

struct usb_hub_descr {
    uint8_t length;
    uint8_t type;
    uint8_t portCount;
    uint16_t flags;
    uint8_t portPowerTime;
    uint8_t current;
} __attribute__((__packed__));

class usbhub : public Bus {
private:
    UsbDeviceInformation usbDeviceInformation;
    usb_hub_descr descr;
public:
    usbhub(Bus &bus, const UsbDeviceInformation &usbDeviceInformation) : Bus("usbhub", &bus), usbDeviceInformation(usbDeviceInformation), descr() {}
    ~usbhub() override;
    void init() override;
};

class usbhub_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};

#endif //JEOKERNEL_USBHUB_H
