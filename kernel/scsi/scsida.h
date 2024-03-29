//
// Created by sigsegv on 3/5/22.
//

#ifndef JEOKERNEL_SCSIDA_H
#define JEOKERNEL_SCSIDA_H

#include <devices/drivers.h>
#include <optional>
#include <core/blockdevsystem.h>
#include <thread>
#include "scsidev.h"
#include "scsi_block.h"

enum scsida_power_state {
    UNKNOWN,
    ACTIVE,
    IDLE,
    STANDBY,
    LOW_POWER
};

class scsida : public Device {
private:
    std::shared_ptr<ScsiDevDeviceInformation> devInfo;
    std::shared_ptr<blockdev_interface> blockdevInterface;
    std::unique_ptr<std::thread> iothr;
    ReadCapacityResult capacity;
    scsida_power_state latestPowerState;
public:
    scsida(Bus *bus, std::shared_ptr<ScsiDevDeviceInformation> devInfo) : Device("scsida", bus), devInfo(devInfo),
    blockdevInterface(), iothr(), latestPowerState(UNKNOWN) {
    }
    ~scsida() override;
    void init() override;
    void stop() override;

    bool activate();
    bool reset();

    void iothread();

    uint64_t NumBlocks() {
        return capacity.LBA + 1;
    }
    uint32_t BlockSize() {
        return capacity.BlockSize;
    }

    void ReportFailed(std::shared_ptr<ScsiDevCommand> command);
    std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const void *buffer);
    std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const scsivariabledata &varlength);
    template <class Cmd> std::shared_ptr<ScsiDevCommand> ExecuteCommand(const Cmd &cmd, std::size_t dataTransferLength, const scsivariabledata &varlength) {
        return ExecuteCommand(&cmd, sizeof(Cmd), dataTransferLength, varlength);
    }
    template <class Cmd> std::shared_ptr<ScsiDevCommand> ExecuteCommand(const Cmd &cmd, std::size_t dataTransferLength, const void *buffer) {
        return ExecuteCommand(&cmd, sizeof(Cmd), dataTransferLength, buffer);
    }
    template <class Cmd, class Result> std::shared_ptr<Result> ExecuteCommand(const Cmd &cmd, const scsivariabledata &varlength) {
        auto command = ExecuteCommand(cmd, sizeof(Result), varlength);
        if (command) {
            if (command->IsSuccessful()) {
                std::shared_ptr<Result> result = std::make_shared<Result>();
                memcpy(&(*result), command->Buffer(), sizeof(Result));
                return result;
            }
            ReportFailed(command);
        }
        return {};
    }
    template <class Cmd> bool ExecuteCommand(const Cmd &cmd) {
        auto command = ExecuteCommand(cmd, 0, scsivariabledata_fixed());
        if (command) {
            if (command->IsSuccessful()) {
                return true;
            }
            ReportFailed(command);
        }
        return false;
    }
    void ReportSense(const RequestSense_FixedData &sense);
    std::shared_ptr<RequestSense_FixedData> RequestSense_Fixed();
    std::optional<bool> IsSpinning();
    bool SetPower(UnitPowerCondition powerCondition, bool immediateResponse = true);
    std::shared_ptr<ScsiDevCommand> CmdRead6(uint32_t LBA, uint16_t blocks);
    std::shared_ptr<ScsiDevCommand> CmdWrite6(uint32_t LBA, uint16_t blocks, const void *buffer);
};

class scsida_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation);
};


#endif //JEOKERNEL_SCSIDA_H
