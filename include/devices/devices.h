//
// Created by sigsegv on 5/19/21.
//

#ifndef JEOKERNEL_DEVICES_H
#define JEOKERNEL_DEVICES_H

#include <string>
#include <vector>
#include <mutex>
#ifndef UNIT_TESTING
#include <klogger.h>
#include <stats/statistics_root.h>

#endif

class Bus;

class DefaultDeviceStatistics : public statistics_object {
public:
    void Accept(statistics_visitor &visitor) override {
    }
};

class Device {
private:
    std::string device_type;
    int device_id;
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
    int DeviceId() {
        return device_id;
    }
    void SetDeviceId(int id) {
        device_id = id;
    }
    virtual void init() {
    }
    virtual void stop() {
    }
    virtual bool IsBus() {
        return false;
    }
    Bus *GetBus() {
        return bus;
    }
    virtual std::shared_ptr<statistics_object> GetStatisticsObject() {
        return std::make_shared<DefaultDeviceStatistics>();
    }
};

class pci;

class Bus : public Device {
private:
    bool have_probed;
public:
    Bus(std::string device_type) : Device(device_type), have_probed(false) {}
    Bus(std::string device_type, Bus *bus) : Device(device_type, bus), have_probed(false) {}
    virtual void ProbeDevices() {
        have_probed = true;
    }
    bool IsBus() override {
        return true;
    }
    virtual bool IsPci() {
        return false;
    }
    virtual pci *GetPci() {
        return nullptr;
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
        device.SetDeviceId(serial);
    }
    void remove(Device &device) {
        auto iterator = devices.begin();
        while (iterator != devices.end()) {
            auto &slot = *iterator;
            if (slot.device == &device) {
                devices.erase(iterator);
                return;
            }
            ++iterator;
        }
    }
};

class Devices : public statistics_object {
private:
    std::mutex mtx;
    std::vector<DeviceGroup> deviceGroups;
public:
    Devices();
    void add(Device &device);
    void remove(Device &device);
    void Accept(statistics_visitor &visitor) override;
};

struct PciDeviceInformation;
struct UsbDeviceInformation;
class ScsiDevDeviceInformation;

struct DeviceInformation {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t device_subclass;
    uint8_t device_class;
    uint8_t prog_if;

    DeviceInformation() : vendor_id(0), device_id(0), device_subclass(0), device_class(0), prog_if(0) {}
    DeviceInformation(const DeviceInformation &) = delete;
    DeviceInformation(DeviceInformation &&) = delete;
    DeviceInformation & operator = (const DeviceInformation &) = default;
    DeviceInformation & operator = (DeviceInformation &&) = delete;

    virtual PciDeviceInformation *GetPciInformation();
    virtual UsbDeviceInformation *GetUsbInformation();
    virtual ScsiDevDeviceInformation *GetScsiDevDeviceInformation();
};

void init_devices();
Devices &devices();

#endif //JEOKERNEL_DEVICES_H
