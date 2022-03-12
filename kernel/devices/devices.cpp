//
// Created by sigsegv on 5/20/21.
//

#include <devices/devices.h>
#include <tuple>
#include "device_group_stats.h"
#include "sstream"

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

void Devices::Accept(statistics_visitor &visitor) {
    std::vector<std::tuple<std::string,std::vector<std::tuple<std::string,std::shared_ptr<statistics_object>>>>> map{};
    {
        std::lock_guard lock{mtx};
        for (auto &group : deviceGroups) {
            std::vector<std::tuple<std::string,std::shared_ptr<statistics_object>>> devices{};
            for (auto &device : group.devices) {
                std::stringstream devname{};
                devname << group.device_type << device.id;
                devices.push_back(std::make_tuple<std::string,std::shared_ptr<statistics_object>>(
                        devname.str(), device.device->GetStatisticsObject()
                ));
            }
            map.push_back(std::make_tuple<std::string,std::vector<std::tuple<std::string,std::shared_ptr<statistics_object>>>>(group.device_type, devices));
        }
    }
    for (auto &group : map) {
        device_group_stats groupLevel{std::get<1>(group)};
        visitor.Visit(std::get<0>(group), groupLevel);
    }
}

Devices::Devices() : deviceGroups() {
}

static std::shared_ptr<Devices> *_devices = nullptr;

void init_drivers();

class devices_stats_factory : public statistics_object_factory {
public:
    std::shared_ptr<statistics_object> GetObject();
};

std::shared_ptr<statistics_object> devices_stats_factory::GetObject() {
    return *_devices;
}

void init_devices() {
    _devices = new std::shared_ptr<Devices>();
    *_devices = new Devices();
    GetStatisticsRoot().Add("devices", std::make_shared<devices_stats_factory>());
    init_drivers();
}

Devices &devices() {
    return **_devices;
}

PciDeviceInformation *DeviceInformation::GetPciInformation() {
    return nullptr;
}
UsbDeviceInformation *DeviceInformation::GetUsbInformation() {
    return nullptr;
}

ScsiDevDeviceInformation *DeviceInformation::GetScsiDevDeviceInformation() {
    return nullptr;
}
