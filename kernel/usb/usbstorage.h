//
// Created by sigsegv on 2/26/22.
//

#ifndef JEOKERNEL_USBSTORAGE_H
#define JEOKERNEL_USBSTORAGE_H

#include <devices/drivers.h>
#include "usbifacedev.h"

class usbstorage : public Device {
private:
    UsbIfacedevInformation devInfo;
    std::shared_ptr<usb_endpoint> bulkInEndpoint;
    std::shared_ptr<usb_endpoint> bulkOutEndpoint;
    uint8_t subclass;
    uint8_t maxLun;
public:
    usbstorage(Bus *bus, UsbIfacedevInformation &devInfo, uint8_t subclass) : Device("usbstorage", bus), devInfo(devInfo), bulkInEndpoint(), bulkOutEndpoint(), subclass(subclass), maxLun(0) {
    }
    ~usbstorage();
    void init() override;
    void stop() override;
    bool MassStorageReset();
    int GetMaxLun();
};

class usbstorage_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};

#endif //JEOKERNEL_USBSTORAGE_H
