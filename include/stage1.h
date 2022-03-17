//
// Created by sigsegv on 3/17/22.
//

#ifndef JEOKERNEL_STAGE1_H
#define JEOKERNEL_STAGE1_H

#include <cstdint>

struct Stage1Data {
    uint32_t multibootAddr;
    uint32_t physpageMapAddr;
};

#endif //JEOKERNEL_STAGE1_H
