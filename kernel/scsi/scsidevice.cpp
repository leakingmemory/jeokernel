//
// Created by sigsegv on 3/2/22.
//

#include <sstream>
#include "scsidevice.h"
#include "scsidev.h"
#include "scsi_primary.h"
#include <concurrency/hw_spinlock.h>
#include <concurrency/critical_section.h>
#include <concurrency/raw_semaphore.h>
#include "CallbackLatch.h"

class scsidevice_scsi_dev : public ScsiDevDeviceInformation {
private:
    scsidevice &device;
    std::shared_ptr<InquiryResult> inquiryResult;
    uint8_t lun;
public:
    scsidevice_scsi_dev(scsidevice &device, std::shared_ptr<InquiryResult> inquiryResult, uint8_t lun) : ScsiDevDeviceInformation(), device(device), inquiryResult(inquiryResult), lun(lun) {
    }
    std::shared_ptr<ScsiDevDeviceInformation> Clone() override;
    ScsiDevDeviceInformation *GetScsiDevDeviceInformation() override;
    uint8_t GetLun() const override;
    std::shared_ptr<InquiryResult> GetInquiryResult() override;
    void SetDevice(Device *device) override;
    std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const scsivariabledata &varlength, const std::function<void ()> &done) override;
};

std::shared_ptr<ScsiDevDeviceInformation> scsidevice_scsi_dev::Clone() {
    return std::make_shared<scsidevice_scsi_dev>(device, inquiryResult, lun);
}

ScsiDevDeviceInformation *scsidevice_scsi_dev::GetScsiDevDeviceInformation() {
    return this;
}

uint8_t scsidevice_scsi_dev::GetLun() const {
    return lun;
}

std::shared_ptr<InquiryResult> scsidevice_scsi_dev::GetInquiryResult() {
    return inquiryResult;
}

void scsidevice_scsi_dev::SetDevice(Device *device) {
    this->device.device = device;
}

std::shared_ptr<ScsiDevCommand> scsidevice_scsi_dev::ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength,
                                         const scsivariabledata &varlength, const std::function<void()> &done) {
    return device.devInfo->ExecuteCommand(cmd, cmdLength, dataTransferLength, varlength, done);
}

scsidevice::~scsidevice() {
    if (device != nullptr) {
        device->stop();
        delete device;
        device = nullptr;
    }
}

void scsidevice::stop() {
    if (device != nullptr) {
        device->stop();
        delete device;
        device = nullptr;
    }
}

void scsidevice::init() {
    auto inquiryResult = DoInquiry();
    if (inquiryResult) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": scsi " << std::hex << inquiryResult->ProductIdentificationLow << ":"
            << inquiryResult->ProductIdentificationHigh << " rev " << inquiryResult->ProductRevisionLevel
            << " vendor " << inquiryResult->VendorIdentification << " perq " << inquiryResult->PeripheralQualifier
            << " peripheral " << inquiryResult->PeripheralDeviceType << "\n";
        get_klogger() << str.str().c_str();

        scsidevice_scsi_dev scsiDev{*this, inquiryResult, devInfo->GetLun()};
        get_drivers().probe(*this, scsiDev);
    } else {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": scsi INQUIRY failed\n";
        get_klogger() << str.str().c_str();
    }
}

std::shared_ptr<InquiryResult> scsidevice::DoInquiry() {
    std::shared_ptr<CallbackLatch> latch = std::make_shared<CallbackLatch>();
    Inquiry inquiry{sizeof(Inquiry_Data), false};
    auto command = devInfo->ExecuteCommand(inquiry, sizeof(Inquiry_Data), [latch] () mutable {
        latch->open();
    });
    if (!latch->wait(10000)) {
        std::stringstream  str{};
        str << DeviceType() << DeviceId() << ": inquiry error: timeout\n";
        get_klogger() << str.str().c_str();
        return {};
    }
    std::shared_ptr<InquiryResult> result{};
    if (command->IsSuccessful()) {
        result = new InquiryResult();
        {
            Inquiry_Data inquiryData{};
            memcpy(&inquiryData, command->Buffer(), sizeof(inquiryData));
            *result = inquiryData.GetResult();
        }
    }
    return result;
}

Device *scsidevice_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    auto *devInfo = deviceInformation.GetScsiDevDeviceInformation();
    if (devInfo != nullptr && !devInfo->GetInquiryResult()) {
        auto *device = new scsidevice(&bus, devInfo->Clone());
        devInfo->SetDevice(device);
        return device;
    }
    return nullptr;
}
