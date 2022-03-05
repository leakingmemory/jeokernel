//
// Created by sigsegv on 3/5/22.
//

#include <sstream>
#include "scsida.h"
#include "scsidev.h"
#include "scsi_primary.h"
#include "concurrency/raw_semaphore.h"

class CallbackLatch {
private:
    std::shared_ptr<raw_semaphore> sema;
public:
    CallbackLatch() : sema(new raw_semaphore(-1)) {}
    void open() {
        sema->release();
    }
    void wait() {
        sema->acquire();
    }
};

scsida::~scsida() noexcept {
}

void scsida::stop() {
}

void scsida::init() {
    auto inquiryResult = devInfo->GetInquiryResult();
    {
        std::unique_ptr<ReadCapacityResult> capacityResult{};
        if (inquiryResult->Protect) {
            ReadCapacity16 cmd{sizeof(ReadCapacity16_Data)};
            auto data = ExecuteCommand<ReadCapacity16,ReadCapacity16_Data>(cmd);
            if (data) {
                if (data->IsValid()) {
                    capacityResult = new ReadCapacityResult(data->GetResult());
                } else {
                    std::stringstream str{};
                    str << DeviceType() << DeviceId() << ": Read capacity(16) invalid response\n";
                    get_klogger() << str.str().c_str();
                }
            } else {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Read capacity(16) failed\n";
                get_klogger() << str.str().c_str();
            }
        } else {
            ReadCapacity10 cmd{};
            auto data = ExecuteCommand<ReadCapacity10,ReadCapacity10_Data>(cmd);
            if (data) {
                if (data->IsValid()) {
                    capacityResult = new ReadCapacityResult(data->GetResult());
                } else {
                    std::stringstream str{};
                    str << DeviceType() << DeviceId() << ": Read capacity(10) invalid response\n";
                    get_klogger() << str.str().c_str();
                }
            } else {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Read capacity(10) failed\n";
                get_klogger() << str.str().c_str();
            }
        }
        if (capacityResult) {
            capacity = *capacityResult;

            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": " << NumBlocks() << " blocks of size " << BlockSize();
            uint64_t size{NumBlocks() * BlockSize()};
            if (size > (10*1024)) {
                size /= 1024;
                if (size > (10*1024)) {
                    size /= 1024;
                    if (size > (10*1024)) {
                        size /= 1024;
                        if (size > (10*1024)) {
                            size /= 1024;
                            str << " = " << size << "TB";
                        } else {
                            str << " = " << size << "GB";
                        }
                    } else {
                        str << " = " << size << "MB";
                    }
                } else {
                    str << " = " << size << "KB";
                }
            } else {
                str << " = " << size << " bytes";
            }
            if (capacity.Extended) {
                str << ", RTO=" << (capacity.ReferenceTagOwnEnable ? "true" : "false") << " ProtInfo="
                    << (capacity.WithProtectionInfo ? "true" : "false");
            }
            str << "\n";
            get_klogger() << str.str().c_str();
        } else {
            return;
        }
    }
}

std::shared_ptr<ScsiDevCommand>
scsida::ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength) {
    CallbackLatch latch{};
    auto command = devInfo->ExecuteCommand(cmd, cmdLength, dataTransferLength, [latch] () mutable {
        latch.open();
    });
    latch.wait();
    return command;
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
