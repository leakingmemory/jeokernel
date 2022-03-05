//
// Created by sigsegv on 3/1/22.
//

#ifndef JEOKERNEL_SCSIDEV_H
#define JEOKERNEL_SCSIDEV_H

#include <devices/devices.h>
#include <functional>

class ScsiDevCommand {
public:
    virtual ~ScsiDevCommand() { }
    virtual const void *Buffer() const = 0;
    virtual std::size_t Size() const = 0;
    virtual bool IsSuccessful() const = 0;
};

class InquiryResult;

class ScsiDevDeviceInformation : public DeviceInformation {
public:
    ScsiDevDeviceInformation() : DeviceInformation() {
    }
    virtual ~ScsiDevDeviceInformation() { }
    virtual std::shared_ptr<ScsiDevDeviceInformation> Clone() = 0;
    virtual ScsiDevDeviceInformation *GetScsiDevDeviceInformation() override = 0;
    virtual uint8_t GetLun() const = 0;
    virtual void SetDevice(Device *device) = 0;
    virtual std::shared_ptr<InquiryResult> GetInquiryResult() = 0;
    virtual std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const std::function<void ()> &done) = 0;
    template <class Cmd> std::shared_ptr<ScsiDevCommand> ExecuteCommand(const Cmd &cmd, std::size_t dataTransferLength, const std::function<void ()> &done) {
        return ExecuteCommand((const void *) &cmd, sizeof(cmd), dataTransferLength, done);
    }
};

#endif //JEOKERNEL_SCSIDEV_H
