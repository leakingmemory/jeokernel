//
// Created by sigsegv on 3/3/22.
//

#ifndef JEOKERNEL_SCSI_PRIMARY_H
#define JEOKERNEL_SCSI_PRIMARY_H

#include <cstdint>
#include <endian.h>

struct Inquiry {
    uint8_t OpCode;
    bool EVPD : 8;
    uint8_t PageCode;
    big_endian<uint16_t> AllocationLength;
    uint8_t Control;

    Inquiry(uint16_t AllocationLength, bool naca) : OpCode(0x12), EVPD(false), PageCode(0),
    AllocationLength(AllocationLength), Control(naca ? 4 : 0) {
    }
} __attribute__((__packed__));

#define SCSI_P_QUALIFIER_ACCESSIBLE                 0
#define SCSI_P_QUALIFIER_ACCESSIBLE_TO_TASK_ROUTER  1
#define SCSI_P_QUALIFIER_NOT_ACCESSIBLE             3

#define SCSI_PERIPHERAL_TYPE_DIRECT_ACCESS  0

#define SCSI_HOTPLUG_NO_INFO    0
#define SCSI_HOTPLUG_INDIVIDUAL 1
#define SCSI_HOTPLUG_DOMAIN     2

struct InquiryResult {
    uint8_t PeripheralQualifier : 3;
    uint8_t PeripheralDeviceType : 5;
    bool RemovableWithoutNexusEvent : 1;
    bool LogicalUnitConglomerate : 1;
    uint8_t HotPluggable : 2;
    uint8_t ResponseDataFormat : 4;
    uint8_t Version;
    bool NormalAca : 1;
    bool HistoricalSupport : 1;
    bool SCCSupported : 1;
    uint8_t TargetPortGroup : 2;
    bool ThirdPartyCopy : 1;
    bool Protect : 1;
    bool EnclosureServices : 1;
    uint8_t AdditionalLength;
    bool MultiPort : 1;
    bool CommandQue : 1;
    uint64_t VendorIdentification;
    uint64_t ProductIdentificationHigh;
    uint64_t ProductIdentificationLow;
    uint32_t ProductRevisionLevel;
};

struct Inquiry_Data {
    uint8_t PeripheralValues;
    uint8_t Flags01;
    uint8_t Version;
    uint8_t Flags03;
    uint8_t AdditionalLength;
    uint8_t Flags05;
    uint8_t Flags06;
    uint8_t Flags07;
    big_endian<uint64_t> VendorIdentification;
    big_endian<uint64_t> ProductIdentificationHigh;
    big_endian<uint64_t> ProductIdentificationLow;
    big_endian<uint32_t> ProductRevisionLevel;

    InquiryResult GetResult() const {
        return {
            .PeripheralQualifier = (uint8_t) (PeripheralValues >> 5),
            .PeripheralDeviceType = (uint8_t) (PeripheralValues & 0x1F),
            .RemovableWithoutNexusEvent = (Flags01 & 0x80) != 0,
            .LogicalUnitConglomerate = (Flags01 & 0x40) != 0,
            .HotPluggable = (uint8_t) ((Flags01 >> 4) & 3),
            .ResponseDataFormat = (uint8_t) (Flags03 & 0x0F),
            .Version = Version,
            .NormalAca = (Flags03 & 0x20) != 0,
            .HistoricalSupport = (Flags03 & 0x10) != 0,
            .SCCSupported = (Flags05 & 0x80) != 0,
            .TargetPortGroup = (uint8_t) ((Flags05 >> 4) & 3),
            .ThirdPartyCopy = (Flags05 & 8) != 0,
            .Protect = (Flags05 & 1) != 0,
            .EnclosureServices = (Flags06 & 0x40) != 0,
            .AdditionalLength = AdditionalLength,
            .MultiPort = (Flags06 & 0x10) != 0,
            .CommandQue = (Flags07 & 2) != 0,
            .VendorIdentification = VendorIdentification,
            .ProductIdentificationHigh = ProductIdentificationHigh,
            .ProductIdentificationLow = ProductIdentificationLow,
            .ProductRevisionLevel = ProductRevisionLevel
        };
    }
};
static_assert(sizeof(Inquiry_Data) == 36);

#endif //JEOKERNEL_SCSI_PRIMARY_H
