//
// Created by sigsegv on 3/5/22.
//

#ifndef JEOKERNEL_SCSIDA_H
#define JEOKERNEL_SCSIDA_H

#include <devices/drivers.h>
#include "scsidev.h"
#include "scsi_block.h"

class scsida : public Device {
private:
    std::shared_ptr<ScsiDevDeviceInformation> devInfo;
    ReadCapacityResult capacity;
public:
    scsida(Bus *bus, std::shared_ptr<ScsiDevDeviceInformation> devInfo) : Device("scsida", bus), devInfo(devInfo) {
    }
    ~scsida();
    void init() override;
    void stop() override;

    uint64_t NumBlocks() {
        return capacity.LBA + 1;
    }
    uint32_t BlockSize() {
        return capacity.BlockSize;
    }

    std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength);
    template <class Cmd> std::shared_ptr<ScsiDevCommand> ExecuteCommand(const Cmd &cmd, std::size_t dataTransferLength) {
        return ExecuteCommand(&cmd, sizeof(Cmd), dataTransferLength);
    }
    template <class Cmd, class Result> std::shared_ptr<Result> ExecuteCommand(const Cmd &cmd) {
        auto command = ExecuteCommand(cmd, sizeof(Result));
        if (command->IsSuccessful()) {
            std::shared_ptr<Result> result = std::make_shared<Result>();
            memcpy(&(*result), command->Buffer(), sizeof(Result));
            return result;
        }
        return {};
    }
};

class scsida_driver : public Driver {
public:
    Device *probe(Bus &bus, DeviceInformation &deviceInformation);
};


#endif //JEOKERNEL_SCSIDA_H
