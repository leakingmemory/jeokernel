//
// Created by sigsegv on 3/2/22.
//

#ifndef JEOKERNEL_SCSIDEVICE_H
#define JEOKERNEL_SCSIDEVICE_H

#include <devices/drivers.h>
#include <functional>
#include "scsi_primary.h"

class scsidevice_scsi_dev;

class scsidevice : public Bus {
    friend scsidevice_scsi_dev;
private:
    std::shared_ptr<ScsiDevDeviceInformation> devInfo;
    Device *device;
public:
    scsidevice(Bus *bus, std::shared_ptr<ScsiDevDeviceInformation> devInfo) : Bus("scsidevice", bus), devInfo(devInfo) {
    }
    ~scsidevice();
    void init() override;
    void stop() override;
    std::shared_ptr<InquiryResult> DoInquiry();
};

class scsidevice_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation);
};


#endif //JEOKERNEL_SCSIDEVICE_H
