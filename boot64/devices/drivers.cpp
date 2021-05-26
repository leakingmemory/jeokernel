//
// Created by sigsegv on 5/25/21.
//

#include <devices/drivers.h>

static Drivers *drivers = nullptr;

bool Drivers::probe(Bus &bus, DeviceInformation &deviceInformation) {
    Device *device;
    {
        std::lock_guard lock{mtx};
        for (Driver *driver : drivers) {
            device = driver->probe(bus, deviceInformation);
            if (device != nullptr) {
                break;
            }
        }
    }
    if (device != nullptr) {
        devices().add(*device);
        device->init();
        return true;
    } else {
        return false;
    }
}

void init_drivers() {
    drivers = new Drivers;
}

Drivers &get_drivers() {
    return *drivers;
}
