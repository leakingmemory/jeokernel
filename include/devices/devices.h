//
// Created by sigsegv on 5/19/21.
//

#ifndef JEOKERNEL_DEVICES_H
#define JEOKERNEL_DEVICES_H

#include <string>
#include <vector>

class Bus;

class Device {
private:
    std::string device_type;
    Bus *bus;
public:
    Device(std::string device_type) : device_type(device_type), bus(nullptr) {}
    Device(std::string device_type, Bus *bus) : Device(device_type) {
        this->bus = bus;
    }
    virtual ~Device() {
    }
    std::string DeviceType() {
        return device_type;
    }
    virtual bool IsBus() {
        return false;
    }
};

class Bus : public Device {
private:
    bool have_probed;
public:
    Bus(std::string device_type) : Device(device_type), have_probed(false) {}
    virtual void ProbeDevices() {
        have_probed = true;
    }
    bool IsBus() override {
        return true;
    }
    bool Probed() {
        return have_probed;
    }
};

struct DeviceSlot {
    int id;
    Device *device;
};

struct DeviceGroup {
    std::string device_type;
    std::vector<DeviceSlot> devices;

    int get_next_serial() const {
        int serial = 0;
        while (true) {
            bool changed{false};
            for (const DeviceSlot &slot : devices) {
                if (slot.id == serial) {
                    ++serial;
                    changed = true;
                }
            }
            if (!changed) {
                break;
            }
        }
        return serial;
    }
    void add(Device &device) {
        int serial = get_next_serial();
        DeviceSlot slot;
        slot.id = serial;
        slot.device = &device;
        devices.push_back(slot);
    }
};

class Devices {
private:
    std::vector<DeviceGroup> deviceGroups;
public:
    Devices();
    void add(Device &device);
};

struct PciDeviceInformation;

struct DeviceInformation {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t device_subclass;
    uint8_t device_class;
    uint8_t prog_if;

    DeviceInformation() {}
    DeviceInformation(const DeviceInformation &) = delete;
    DeviceInformation(DeviceInformation &&) = delete;
    DeviceInformation & operator = (const DeviceInformation &) = default;
    DeviceInformation & operator = (DeviceInformation &&) = delete;

    virtual PciDeviceInformation *GetPciInformation();
};

void init_devices();
Devices &devices();

#endif //JEOKERNEL_DEVICES_H
