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

enum class SenseError {
    NO_SENSE,
    INVALID_OPCODE,
    ATTEMPT_TO_ACCESS_GAP_ZONE,
    ATTEMPT_TO_READ_INVALID_DATA,
    INVALID_ADDRESS_FOR_WRITE,
    INVALID_ELEMENT_ADDRESS,
    INVALID_WRITE_CROSSING_LAYER_JUMP,
    LBA_OUT_OF_RANGE,
    MISALIGNED_WRITE_COMMAND,
    READ_BOUNDARY_VIOLATION,
    UNALIGNED_WRITE_COMMAND,
    WRITE_BOUNDARY_VIOLATION,
    OTHER
};

constexpr const char *SenseErrorString(SenseError error) {
    switch (error) {
        case SenseError::NO_SENSE:
            return "NO_SENSE";
        case SenseError::INVALID_OPCODE:
            return "INVALID_OPCODE";
        case SenseError::ATTEMPT_TO_ACCESS_GAP_ZONE:
            return "ATTEMPT_TO_ACCESS_GAP_ZONE";
        case SenseError::ATTEMPT_TO_READ_INVALID_DATA:
            return "ATTEMPT_TO_READ_INVALID_DATA";
        case SenseError::INVALID_ADDRESS_FOR_WRITE:
            return "INVALID_ADDRESS_FOR_WRITE";
        case SenseError::INVALID_ELEMENT_ADDRESS:
            return "INVALID_ELEMENT_ADDRESS";
        case SenseError::INVALID_WRITE_CROSSING_LAYER_JUMP:
            return "INVALID_WRITE_CROSSING_LAYER_JUMP";
        case SenseError::LBA_OUT_OF_RANGE:
            return "LBA_OUT_OF_RANGE";
        case SenseError::MISALIGNED_WRITE_COMMAND:
            return "MISALIGNED_WRITE_COMMAND";
        case SenseError::READ_BOUNDARY_VIOLATION:
            return "READ_BOUNDARY_VIOLATION";
        case SenseError::UNALIGNED_WRITE_COMMAND:
            return "UNALIGNED_WRITE_COMMAND";
        case SenseError::WRITE_BOUNDARY_VIOLATION:
            return "WRITE_BOUNDARY_VIOLATION";
        case SenseError::OTHER:
        default:
            return "OTHER";
    }
}

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
    SenseError SenseError() const {
        auto senseKey = SenseKey();
        switch (senseKey) {
            case SENSE_KEY_NO_SENSE:
                return SenseError::NO_SENSE;
            case SENSE_KEY_ILLEGAL_REQUEST:
            {
                switch (AdditionalSenseCode) {
                    case 0x20:
                    {
                        switch (AdditionalSenseCodeQualifier) {
                            case 0:
                                return SenseError::INVALID_OPCODE;
                        }
                    }
                    case 0x21:
                    {
                        switch (AdditionalSenseCodeQualifier) {
                            case 0x09:
                                return SenseError::ATTEMPT_TO_ACCESS_GAP_ZONE;
                            case 0x06:
                                return SenseError::ATTEMPT_TO_READ_INVALID_DATA;
                            case 0x02:
                                return SenseError::INVALID_ADDRESS_FOR_WRITE;
                            case 0x01:
                                return SenseError::INVALID_ELEMENT_ADDRESS;
                            case 0x03:
                                return SenseError::INVALID_WRITE_CROSSING_LAYER_JUMP;
                            case 0x00:
                                return SenseError::LBA_OUT_OF_RANGE;
                            case 0x08:
                                return SenseError::MISALIGNED_WRITE_COMMAND;
                            case 0x07:
                                return SenseError::READ_BOUNDARY_VIOLATION;
                            case 0x04:
                                return SenseError::UNALIGNED_WRITE_COMMAND;
                            case 0x05:
                                return SenseError::WRITE_BOUNDARY_VIOLATION;
                        }
                    }
                    break;
                }
            }
            break;
        }
        return SenseError::OTHER;
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

enum class UnitPowerCondition {
    LOAD,
    EJECT,
    STOP,
    ACTIVE,
    IDLE,
    STANDBY,
    LU_CONTROL,
    FORCE_IDLE_0,
    FORCE_STANDBY_0
};

struct StartStopUnit {
    uint8_t OpCode;
    bool Immediate : 8;
    uint16_t Zero;
    uint8_t PowerCondition;
    uint8_t Control;

    StartStopUnit(bool immediate, UnitPowerCondition unitPowerCondition, bool naca = false) : OpCode(0x1B),
    Immediate(immediate), Zero(0), PowerCondition(EncodePowerCondition(unitPowerCondition)),
    Control(naca ? SCSI_CONTROL_NACA : 0) {}

    static constexpr uint8_t EncodePowerCondition(UnitPowerCondition unitPowerCondition) {
        switch (unitPowerCondition) {
            case UnitPowerCondition::LOAD:
                return 3;
            case UnitPowerCondition::EJECT:
                return 2;
            case UnitPowerCondition::STOP:
                return 0;
            case UnitPowerCondition::ACTIVE:
                return 1;
            default:
                return PowerConditionValue(unitPowerCondition) << 4;
        }
    }
    static constexpr uint8_t PowerConditionValue(UnitPowerCondition unitPowerCondition) {
        switch (unitPowerCondition) {
            case UnitPowerCondition::IDLE:
                return 2;
            case UnitPowerCondition::STANDBY:
                return 3;
            case UnitPowerCondition::LU_CONTROL:
                return 7;
            case UnitPowerCondition::FORCE_IDLE_0:
                return 0xA;
            case UnitPowerCondition::FORCE_STANDBY_0:
                return 0xB;
            default:
                return 0;
        }
    }
} __attribute__((__packed__));

struct Read6 {
    uint8_t OpCode;
    uint8_t LBA_high_5bits;
    big_endian<uint16_t> LBA_low;
    uint8_t TransferLengthBlocks0is256;
    uint8_t Control;

    Read6(uint32_t LBA, uint16_t TransferLengthBlocks, bool naca = false) : OpCode(0x08),
    LBA_high_5bits((uint8_t) ((LBA >> 16) & 0x01F)), LBA_low((uint16_t) (LBA & 0x0FFFF)),
    TransferLengthBlocks0is256(TransferLengthBlocks != 256 ? ((uint8_t) (TransferLengthBlocks & 0x0FF)) : 0),
    Control(naca ? SCSI_CONTROL_NACA : 0) {}

    uint32_t LBA() {
        uint32_t LBA_n{(uint32_t) (LBA_high_5bits & 0x1F)};
        LBA_n = LBA_n << 16;
        LBA_n |= LBA_low;
        return LBA_n;
    }
    uint16_t TransferLengthBlocks() {
        uint16_t blocks{(uint16_t) TransferLengthBlocks0is256};
        return blocks != 0 ? blocks : 256;
    }
} __attribute__((__packed__));

#endif //JEOKERNEL_SCSI_BLOCK_H
