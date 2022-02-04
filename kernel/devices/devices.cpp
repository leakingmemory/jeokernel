//
// Created by sigsegv on 5/20/21.
//

#include <devices/devices.h>

void Devices::add(Device &device) {
    std::lock_guard lock{mtx};
    std::string devtype = device.DeviceType();
    for (DeviceGroup &grp : deviceGroups) {
        if (grp.device_type == devtype) {
            grp.add(device);
            return;
        }
    }
    DeviceGroup deviceGroup{};
    deviceGroup.device_type = device.DeviceType();
    deviceGroup.add(device);
    deviceGroups.push_back(deviceGroup);
}

void Devices::remove(Device &device) {
    device.stop();
    std::lock_guard lock{mtx};
    for (DeviceGroup &grp : deviceGroups) {
        grp.remove(device);
    }
}

Devices::Devices() : deviceGroups() {
}

static Devices *_devices = nullptr;

void init_drivers();

void init_devices() {
    _devices = new Devices();
    init_drivers();
}

Devices &devices() {
    return *_devices;
}

PciDeviceInformation *DeviceInformation::GetPciInformation() {
    return nullptr;
}
UsbDeviceInformation *DeviceInformation::GetUsbInformation() {
    return nullptr;
}
