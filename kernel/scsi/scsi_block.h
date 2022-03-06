//
// Created by sigsegv on 3/5/22.
//

#ifndef JEOKERNEL_SCSI_BLOCK_H
#define JEOKERNEL_SCSI_BLOCK_H

#include "scsi_primary.h"

struct ReadCapacityResult {
    uint64_t LBA;
    uint32_t BlockSize;
    bool Extended;
    bool ReferenceTagOwnEnable;
    bool WithProtectionInfo;
};

struct ReadCapacity10 {
    uint8_t OpCode;
    uint8_t Zero0;
    big_endian<uint32_t> LBA;
    uint16_t Zero1;
    bool PMI : 8;
    uint8_t Control;

    ReadCapacity10(bool naca = false) : OpCode(0x25), Zero0(0), LBA(0), Zero1(0), PMI(false),
    Control(naca ? SCSI_CONTROL_NACA : 0) {}
} __attribute__((__packed__));

static_assert(sizeof(ReadCapacity10) == 10);

struct ReadCapacity10_Data {
    big_endian<uint32_t> LBA;
    big_endian<uint32_t> BlockLength;

    bool IsValid() {
        return LBA != 0xFFFFFFFF && BlockLength != 0xFFFFFFFF;
    }
    ReadCapacityResult GetResult() {
        return {
            .LBA = LBA,
            .BlockSize = BlockLength,
            .Extended = false,
            .ReferenceTagOwnEnable = false,
            .WithProtectionInfo = false
        };
    }
} __attribute__((__packed__));

static_assert(sizeof(ReadCapacity10_Data) == 8);

struct ReadCapacity16 {
    uint8_t OpCode;
    uint8_t ServiceAction;
    big_endian<uint64_t> LBA;
    big_endian<uint32_t> AllocationLength;
    bool PMI : 8;
    uint8_t Control;

    ReadCapacity16(uint32_t allocationLength, bool naca = false) : OpCode(0x9E), ServiceAction(0x10), LBA(0),
    AllocationLength(allocationLength), PMI(false), Control(naca ? SCSI_CONTROL_NACA : 0) {}
} __attribute__((__packed__));

static_assert(sizeof(ReadCapacity16) == 16);

struct ReadCapacity16_Data {
    big_endian<uint64_t> LBA;
    big_endian<uint32_t> BlockLength;
    uint8_t RtoAndProt;
    uint8_t Reserved[19];

    bool IsValid() {
        return LBA != 0xFFFFFFFFFFFFFFFF && BlockLength != 0xFFFFFFFF;
    }
    ReadCapacityResult GetResult() {
        return {
                .LBA = LBA,
                .BlockSize = BlockLength,
                .Extended = true,
                .ReferenceTagOwnEnable = (RtoAndProt & 2) != 0,
                .WithProtectionInfo = (RtoAndProt & 1) != 0
        };
    }
} __attribute__((__packed__));

static_assert(sizeof(ReadCapacity16_Data) == 32);

#define REQUEST_SENSE_FIXED_DATA_MAX_LENGTH 252

struct RequestSense {
    uint8_t OpCode;
    bool DescriptorFormat : 8;
    uint16_t Zero;
    uint8_t AllocationLength;
    uint8_t Control;

    RequestSense(bool DescriptorFormat = false, bool naca = false) : OpCode(0x03), DescriptorFormat(DescriptorFormat), Zero(0),
    AllocationLength(REQUEST_SENSE_FIXED_DATA_MAX_LENGTH), Control(naca ? SCSI_CONTROL_NACA : 0) {}
} __attribute__((__packed__));

static_assert(sizeof(RequestSense) == 6);

#define SENSE_KEY_NO_SENSE          0
#define SENSE_KEY_RECOVERED_ERROR   1
#define SENSE_KEY_NOT_READY         2
#define SENSE_KEY_MEDIUM_ERROR      3
#define SENSE_KEY_HARDWARE_ERROR    4
#define SENSE_KEY_ILLEGAL_REQUEST   5
#define SENSE_KEY_UNIT_ATTENTION    6
#define SENSE_KEY_DATA_PROTECT      7
#define SENSE_KEY_BLANK_CHECK       8
#define SENSE_KEY_VENDOR_SPECIFIC   9
#define SENSE_KEY_COPY_ABORTED      0xA
#define SENSE_KEY_ABORTED_COMMAND   0xB
#define SENSE_KEY_RESERVED_C        0xC
#define SENSE_KEY_VOLUME_OVERFLOW   0xD
#define SENSE_KEY_MISCOMPARE        0xE
#define SENSE_KEY_COMPLETED         0xF

#define SENSE_KEYS_MAX 16
constexpr const char *SenseKeyStr(uint8_t senseCode) {
    if (senseCode >= SENSE_KEYS_MAX) {
        return "INVALID_SENSE_KEY";
    }
    const char *array[SENSE_KEYS_MAX] = {
        "NO_SENSE",
        "RECOVERED_ERROR",
        "NOT_READY",
        "MEDIUM_ERROR",
        "HARDWARE_ERROR",
        "ILLEGAL_REQUEST",
        "UNIT_ATTENTION",
        "DATA_PROTECT",
        "BLANK_CHECK",
        "VENDOR_SPECIFIC",
        "COPY_ABORTED",
        "ABORTED_COMMAND",
        "RESERVED_C",
        "VOLUME_OVERFLOW",
        "MISCOMPARE",
        "COMPLETED"
    };
    return array[senseCode];
}

#define NO_SENSE_POWER_CONDITION 0x5E

#define SENSE_Q_POWER_LOW_POWER                     0x00
#define SENSE_Q_POWER_IDLE_BY_COMMAND               0x03
#define SENSE_Q_POWER_IDLE_BY_TIMER                 0x01
#define SENSE_Q_POWER_IDLE_B_BY_COMMAND             0x06
#define SENSE_Q_POWER_IDLE_B_BY_TIMER               0x05
#define SENSE_Q_POWER_IDLE_C_BY_COMMAND             0x08
#define SENSE_Q_POWER_IDLE_C_BY_TIMER               0x07
#define SENSE_Q_POWER_STATE_CHANGE_TO_ACTIVE        0x41
#define SENSE_Q_POWER_STATE_CHANGE_TO_DEVCONTROL    0x47
#define SENSE_Q_POWER_STATE_CHANGE_TO_IDLE          0x42
#define SENSE_Q_POWER_STATE_CHANGE_TO_SLEEP         0x45
#define SENSE_Q_POWER_STATE_CHANGE_TO_STANDBY       0x43
#define SENSE_Q_POWER_STANDBY_BY_COMMAND            0x04
#define SENSE_Q_POWER_STANDBY_BY_TIMER              0x02
#define SENSE_Q_POWER_STANDBY_Y_BY_COMMAND          0x0A
#define SENSE_Q_POWER_STANDBY_Y_BY_TIMER            0x09


struct RequestSense_FixedData {
    uint8_t ResponseCode;
    uint8_t Reserved;
    uint8_t FlagsAndSenseKey;
    big_endian<uint32_t> Information;
    uint8_t AdditionalSenseLength;
    big_endian<uint32_t> CommandSpecificInformation;
    uint8_t AdditionalSenseCode;
    uint8_t AdditionalSenseCodeQualifier;
    uint8_t FieldReplaceableUnitCode;
    uint8_t SenseKeySpecificInformation[3];
    uint8_t AdditionalSenseBytes[234];

    uint8_t SenseKey() const {
        return FlagsAndSenseKey & 0xF;
    }
} __attribute__((__packed__));

static_assert(sizeof(RequestSense_FixedData) == REQUEST_SENSE_FIXED_DATA_MAX_LENGTH);

class RequestSense_FixedData_VariableLength : public scsivariabledata {
public:
    RequestSense_FixedData_VariableLength() : scsivariabledata(8) {}
    std::unique_ptr<scsivariabledata> clone() const override {
        return std::unique_ptr<scsivariabledata>(new RequestSense_FixedData_VariableLength());
    }
    std::size_t Remaining(const void *ptr, std::size_t initialRead) const {
        RequestSense_FixedData data{};
        memcpy(&data, ptr, 8);
        std::size_t totalRead{(std::size_t) (data.AdditionalSenseLength + 8)};
        if (totalRead > initialRead) {
            return totalRead - initialRead;
        } else {
            return 0;
        }
    }
};

#endif //JEOKERNEL_SCSI_BLOCK_H
