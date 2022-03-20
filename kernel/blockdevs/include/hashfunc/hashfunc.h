//
// Created by sigsegv on 2/23/22.
//

#ifndef FSBITS_HASHFUNC_H
#define FSBITS_HASHFUNC_H

#include <cstdint>

class byte_hashfunc {
public:
    virtual ~byte_hashfunc();
    virtual void encode(uint8_t byte) = 0;
    void encode(const void *data, std::size_t n);
};

#endif //FSBITS_HASHFUNC_H
