//
// Created by sigsegv on 3/5/22.
//

#include <sstream>
#include <blockdevs/blockdev_block.h>
#include "scsida.h"
#include "scsidev.h"
#include "scsi_primary.h"
#include <concurrency/raw_semaphore.h>
#include <thread>

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

scsida::~scsida() {
    if (blockdevInterface) {
        blockdevInterface->Stop();
    }
    get_blockdevsystem().Remove(&(*blockdevInterface));
    if (iothr) {
        iothr->join();
        iothr = {};
    }
}

void scsida::stop() {
    if (blockdevInterface) {
        blockdevInterface->Stop();
    }
}

void scsida::init() {
    auto inquiryResult = devInfo->GetInquiryResult();
    {
        std::unique_ptr<ReadCapacityResult> capacityResult{};
        if (inquiryResult->Protect) {
            ReadCapacity16 cmd{sizeof(ReadCapacity16_Data)};
            auto data = ExecuteCommand<ReadCapacity16,ReadCapacity16_Data>(cmd, scsivariabledata_fixed());
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
            auto data = ExecuteCommand<ReadCapacity10,ReadCapacity10_Data>(cmd, scsivariabledata_fixed());
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

        auto &blockdevsys = get_blockdevsystem();
        blockdevInterface = blockdevsys.CreateInterface();
        blockdevInterface->SetBlocksize(BlockSize());

        iothr = std::make_unique<std::thread>([this] () {
            {
                std::stringstream str{};
                str << "[" << DeviceType() << DeviceId() << "]";
                std::this_thread::set_name(str.str());
            }
            iothread();
        });

        {
            std::stringstream name{};
            name << DeviceType() << DeviceId();
            blockdevsys.Add(name.str(), &(*blockdevInterface));
        }
    }

    SetPower(UnitPowerCondition::ACTIVE);
    {
        auto spinInfo = IsSpinning();
        if (spinInfo) {
            bool isSpinning = *spinInfo;
            if (isSpinning) {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": is active/powered\n";
                get_klogger() << str.str().c_str();
            } else {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": did not activate: ";
                switch (latestPowerState) {
                    case IDLE:
                        str << "idle";
                        break;
                    case STANDBY:
                        str << "standby";
                        break;
                    case LOW_POWER:
                        str << "low power state";
                        break;
                    default:
                        str << "invalid";
                }
                str << "\n";
                get_klogger() << str.str().c_str();
                return;
            }
        }
    }
}

class scsida_result : public blockdev_block {
private:
    std::shared_ptr<ScsiDevCommand> command;
public:
    scsida_result(std::shared_ptr<ScsiDevCommand> command) : command(command) {}
    void *Pointer() const override;
    std::size_t Size() const override;
};

void *scsida_result::Pointer() const {
    return (void *) command->Buffer();
}

std::size_t scsida_result::Size() const {
    return command->Size();
}

void scsida::iothread() {
    while (true) {
        auto cmd = blockdevInterface->NextCommand();
        if (!cmd) {
            break;
        }
        auto result = CmdRead6(cmd->Blocknum(), cmd->Blocks());
        if (result) {
            std::shared_ptr<blockdev_block> wrap = std::make_shared<scsida_result>(result);
            cmd->Accept(wrap);
        }
    }
}

void scsida::ReportFailed(std::shared_ptr<ScsiDevCommand> command) {
    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": Command failed: " << command->NonSuccessfulStatusString() << "\n";
    get_klogger() << str.str().c_str();
    if (command->NonSuccessfulStatus() == ScsiCmdNonSuccessfulStatus::COMMAND_FAILED) {
        auto sense = RequestSense_Fixed();
        if (sense) {
            ReportSense(*sense);
        }
    }
}

std::shared_ptr<ScsiDevCommand>
scsida::ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const scsivariabledata &varlength) {
    CallbackLatch latch{};
    auto command = devInfo->ExecuteCommand(cmd, cmdLength, dataTransferLength, varlength, [latch] () mutable {
        latch.open();
    });
    latch.wait();
    return command;
}

void scsida::ReportSense(const RequestSense_FixedData &sense) {
    auto senseError = sense.SenseError();
    if (senseError != SenseError::OTHER) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Sense error " << SenseErrorString(senseError) << "\n";
        get_klogger() << str.str().c_str();
        return;
    }
    auto senseKey = sense.SenseKey();
    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": Sense " << SenseKeyStr(senseKey) << " code " << std::hex << sense.AdditionalSenseCode << " q " << sense.AdditionalSenseCodeQualifier << "\n";
    get_klogger() << str.str().c_str();
}

std::shared_ptr<RequestSense_FixedData> scsida::RequestSense_Fixed() {
    RequestSense requestSense{};
    std::shared_ptr<RequestSense_FixedData> shptr = ExecuteCommand<RequestSense,RequestSense_FixedData>(
            requestSense, RequestSense_FixedData_VariableLength()
    );
    return shptr;
}

std::optional<bool> scsida::IsSpinning() {
    int errc{5};
    auto reqsens = RequestSense_Fixed();
    while (true) {
        if (!reqsens) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Request sense failed\n";
            get_klogger() << str.str().c_str();
            latestPowerState = UNKNOWN;
            return {};
        }
        auto senseKey = reqsens->SenseKey();
        if (senseKey == SENSE_KEY_NO_SENSE) {
            bool powerTransitioning{false};
            if (reqsens->AdditionalSenseCode == NO_SENSE_POWER_CONDITION) {
                switch (reqsens->AdditionalSenseCodeQualifier) {
                    case SENSE_Q_POWER_STATE_CHANGE_TO_ACTIVE:
                    case SENSE_Q_POWER_STATE_CHANGE_TO_DEVCONTROL:
                    case SENSE_Q_POWER_STATE_CHANGE_TO_IDLE:
                    case SENSE_Q_POWER_STATE_CHANGE_TO_SLEEP:
                    case SENSE_Q_POWER_STATE_CHANGE_TO_STANDBY:
                        powerTransitioning = true;
                        break;
                }
            }
            if (!powerTransitioning) {
                break;
            }

            std::stringstream str{};
            str << DeviceType() << DeviceId() << "Sense: Power state transition in progress\n";
            get_klogger() << str.str().c_str();

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(500ms);

            reqsens = RequestSense_Fixed();
            continue;
        }
        ReportSense(*reqsens);
        if (--errc == 0) {
            latestPowerState = UNKNOWN;
            return {};
        }

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(500ms);

        reqsens = RequestSense_Fixed();
    }

    if (reqsens->AdditionalSenseCode == NO_SENSE_POWER_CONDITION) {
        switch (reqsens->AdditionalSenseCodeQualifier) {
            case SENSE_Q_POWER_LOW_POWER:
                latestPowerState = LOW_POWER;
                return false;
            case SENSE_Q_POWER_IDLE_BY_COMMAND:
            case SENSE_Q_POWER_IDLE_BY_TIMER:
            case SENSE_Q_POWER_IDLE_B_BY_COMMAND:
            case SENSE_Q_POWER_IDLE_B_BY_TIMER:
            case SENSE_Q_POWER_IDLE_C_BY_COMMAND:
            case SENSE_Q_POWER_IDLE_C_BY_TIMER:
                latestPowerState = IDLE;
                return false;
            case SENSE_Q_POWER_STANDBY_BY_COMMAND:
            case SENSE_Q_POWER_STANDBY_BY_TIMER:
            case SENSE_Q_POWER_STANDBY_Y_BY_COMMAND:
            case SENSE_Q_POWER_STANDBY_Y_BY_TIMER:
                latestPowerState = STANDBY;
                return false;
            default:
                ReportSense(*reqsens);
                latestPowerState = UNKNOWN;
                return {};
        }
    }

    latestPowerState = ACTIVE;
    return true;
}

bool scsida::SetPower(UnitPowerCondition powerCondition, bool immediateResponse) {
    StartStopUnit setPower{immediateResponse, powerCondition};
    if (!ExecuteCommand(setPower)) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Set power failed\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    return true;
}

std::shared_ptr<ScsiDevCommand> scsida::CmdRead6(uint32_t LBA, uint16_t blocks) {
    Read6 read6{LBA, blocks};
    if (read6.LBA() != LBA || read6.TransferLengthBlocks() != blocks) {
        return {};
    }
    std::size_t size{blocks};
    size *= BlockSize();
    auto command = ExecuteCommand(read6, size, scsivariabledata_fixed());
    if (command) {
        if (command->IsSuccessful()) {
            return command;
        } else {
            ReportFailed(command);
        }
    }
    return {};
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
