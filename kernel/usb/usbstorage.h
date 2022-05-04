//
// Created by sigsegv on 2/26/22.
//

#ifndef JEOKERNEL_USBSTORAGE_H
#define JEOKERNEL_USBSTORAGE_H

#include <devices/drivers.h>
#include <vector>
#include "usbifacedev.h"
#include "../scsi/scsierror.h"
#include "../scsi/scsivariabledata.h"
#include <concurrency/raw_semaphore.h>

class usbstorage_command_impl;

class usbstorage_command {
    friend usbstorage_command_impl;
private:
    uint8_t cmd[16];
    std::function<void ()> done;
    std::unique_ptr<scsivariabledata> varlength;
    uint32_t dataTransferLength;
    uint8_t lun;
    uint8_t cmdLength;
    bool inbound : 8;
    bool hasStatus : 8;
    ScsiCmdNonSuccessfulStatus nonSuccessfulStatus;
    const char *nonSuccessfulStatusString;
public:
    usbstorage_command(uint32_t dataTransferLength, const scsivariabledata &varlength, uint8_t lun, const void *cmd, uint8_t cmdLength, bool inbound,
                       const std::function<void ()> &done)
    : cmd(), done(done), varlength(), dataTransferLength(dataTransferLength), lun(lun), cmdLength(cmdLength),
    inbound(inbound), hasStatus(false), nonSuccessfulStatus(ScsiCmdNonSuccessfulStatus::UNSPECIFIED),
    nonSuccessfulStatusString("UNSPECIFIED") {
        memcpy((void *) &(this->cmd[0]), cmd, cmdLength);
        this->varlength = varlength.clone();
    }
    usbstorage_command(const usbstorage_command &) = delete;
    usbstorage_command(usbstorage_command &&) = delete;
    usbstorage_command &operator =(const usbstorage_command &) = delete;
    usbstorage_command &operator =(usbstorage_command &&) = delete;
    virtual ~usbstorage_command() {}

    virtual const void *Buffer() const = 0;
    virtual bool IsSuccessful() = 0;
    std::size_t DataTransferLength() const {
        return dataTransferLength;
    }
    bool HasStatus() const {
        return hasStatus;
    }
    ScsiCmdNonSuccessfulStatus NonSuccessfulStatus() const {
        return nonSuccessfulStatus;
    }
    const char *NonSucessfulStatusString() const {
        return nonSuccessfulStatusString;
    }
};

class usbstorage : public Bus {
    friend usbstorage_command_impl;
private:
    UsbIfacedevInformation devInfo;
    usb_endpoint_descriptor bulkInDesc;
    usb_endpoint_descriptor bulkOutDesc;
    std::shared_ptr<usb_endpoint> bulkInEndpoint;
    std::shared_ptr<usb_endpoint> bulkOutEndpoint;
    std::vector<std::shared_ptr<usbstorage_command_impl>> commandQueue;
    std::shared_ptr<usbstorage_command_impl> currentCommand;
    std::vector<std::function<void ()>> taskQueue;
    std::thread *taskRunner;
    hw_spinlock taskQueueLock;
    raw_semaphore taskAvailable;
    Device *device;
    uint32_t commandTag;
    uint8_t subclass;
    uint8_t maxLun;
    uint8_t bulkOutToggle;
    uint8_t bulkInToggle;
    bool stopThreads;
public:
    usbstorage(Bus *bus, UsbIfacedevInformation &devInfo, uint8_t subclass) : Bus("usbstorage", bus), devInfo(devInfo),
    bulkInDesc(), bulkOutDesc(), bulkInEndpoint(), bulkOutEndpoint(), commandQueue(), currentCommand(), taskQueue(),
    taskRunner(nullptr), taskQueueLock(), taskAvailable(-1), device(nullptr), commandTag(0), subclass(subclass),
    maxLun(0), bulkOutToggle(0), bulkInToggle(0), stopThreads(false) {
    }
    ~usbstorage();
    void init() override;
    void stop() override;
    void SetDevice(Device *device);
    std::shared_ptr<usbstorage_command> QueueCommand(uint32_t dataTransferLength, const scsivariabledata &varlength, uint8_t lun, const void *cmd, uint8_t cmdLength, const std::function<void ()> &done);
    template <class T> std::shared_ptr<usbstorage_command> QueueCommand(uint32_t dataTransferLength, uint8_t lun, const T &cmd, const std::function<void ()> &done) {
        static_assert(sizeof(T) <= 16);
        return QueueCommand(dataTransferLength, scsivariabledata_fixed(), lun, (const void *) &cmd, sizeof(T), done);
    }
    bool ResetDevice();
    void QueueTask(const std::function<void ()> &task);
private:
    bool MassStorageReset();
    int GetMaxLun();
    void ExecuteQueueItem();
    void RunTasks();
};

class usbstorage_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};

#endif //JEOKERNEL_USBSTORAGE_H
