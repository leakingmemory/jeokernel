//
// Created by sigsegv on 3/5/22.
//

#include <sstream>
#include "scsida.h"
#include "scsidev.h"
#include "scsi_primary.h"

scsida::~scsida() noexcept {
}

void scsida::stop() {
}

void scsida::init() {
    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": init\n";
    get_klogger() << str.str().c_str();
}

Device *scsida_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    auto *devInfo = deviceInformation.GetScsiDevDeviceInformation();
    if (devInfo != nullptr) {
        auto inquiryResult = devInfo->GetInquiryResult();
        if (
                inquiryResult &&
                inquiryResult->PeripheralQualifier == SCSI_P_QUALIFIER_ACCESSIBLE &&
                inquiryResult->PeripheralDeviceType == SCSI_PERIPHERAL_TYPE_DIRECT_ACCESS
           ) {
            auto *device = new scsida(&bus, devInfo->Clone());
            devInfo->SetDevice(device);
            return device;
        }
    }
    return nullptr;
}
