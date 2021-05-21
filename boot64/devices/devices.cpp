//
// Created by sigsegv on 5/20/21.
//

#include <devices/devices.h>

void Devices::add(Device &device) {
    std::string devtype = device.DeviceType();
    for (DeviceGroup &grp : deviceGroups) {
        if (grp.device_type == devtype) {
            grp.add(device);
        }
    }
}

Devices::Devices() : deviceGroups() {
}

static Devices *_devices = nullptr;

void init_devices() {
    _devices = new Devices();
}

Devices &devices() {
    return *_devices;
}

PciDeviceInformation *DeviceInformation::GetPciInformation() {
    return nullptr;
}
