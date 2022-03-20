//
// Created by sigsegv on 2/23/22.
//

#ifndef FSBITS_CRC32_H
#define FSBITS_CRC32_H

#include <hashfunc/hashfunc.h>

class crc32 : public byte_hashfunc {
private:
    uint32_t state;
public:
    crc32();
    void encode(uint8_t byte);
    uint32_t complete();
};

#endif //FSBITS_CRC32_H
