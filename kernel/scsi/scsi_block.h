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

#endif //JEOKERNEL_SCSI_BLOCK_H
