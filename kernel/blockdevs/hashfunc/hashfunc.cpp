//
// Created by sigsegv on 2/23/22.
//

#include <hashfunc/hashfunc.h>

byte_hashfunc::~byte_hashfunc() {
}

void byte_hashfunc::encode(const void *data, std::size_t n) {
    const uint8_t *bytep = (const uint8_t *) data;
    for (std::size_t i = 0; i < n; i++) {
        encode(bytep[i]);
    }
}