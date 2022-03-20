//
// Created by sigsegv on 2/9/22.
//

#ifndef FSBITS_BLOCKDEV_BLOCK_H
#define FSBITS_BLOCKDEV_BLOCK_H

#include <cstdint>

class blockdev_block {
public:
    virtual ~blockdev_block() { }
    virtual void *Pointer() const = 0;
    virtual std::size_t Size() const = 0;
};

#endif //FSBITS_BLOCKDEV_BLOCK_H
