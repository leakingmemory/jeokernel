//
// Created by sigsegv on 3/5/22.
//

#ifndef JEOKERNEL_SCSIDA_H
#define JEOKERNEL_SCSIDA_H

#include <devices/drivers.h>

class scsida : public Device {
private:
    std::shared_ptr<ScsiDevDeviceInformation> devInfo;
public:
    scsida(Bus *bus, std::shared_ptr<ScsiDevDeviceInformation> devInfo) : Device("scsida", bus), devInfo(devInfo) {
    }
    ~scsida();
    void init() override;
    void stop() override;
};

class scsida_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation);
};


#endif //JEOKERNEL_SCSIDA_H
