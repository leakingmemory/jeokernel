//
// Created by sigsegv on 5/25/21.
//

#ifndef JEOKERNEL_DRIVERS_H
#define JEOKERNEL_DRIVERS_H

#include <mutex>

#ifndef UNIT_TESTING
#include <devices/devices.h>
#endif

class Driver {
public:
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) {
        return nullptr;
    }
};

class Drivers {
private:
    std::mutex mtx;
    std::vector<Driver *> drivers;
public:
    Drivers() : drivers() {
    }
    void AddDriver(Driver *driver) {
        std::lock_guard lock{mtx};
        drivers.push_back(driver);
    }
    bool probe(Bus &bus, DeviceInformation &deviceInformation);
};

Drivers &get_drivers();

#endif //JEOKERNEL_DRIVERS_H
