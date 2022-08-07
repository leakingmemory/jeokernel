//
// Created by sigsegv on 8/7/22.
//

#ifndef JEOKERNEL_PRNG_H
#define JEOKERNEL_PRNG_H

#include <cstdint>
#include <memory>

class prng {
public:
    virtual ~prng() = default;
    virtual void Seed(const void *buf, size_t len) = 0;
    virtual void Generate(void *buf, size_t len) = 0;
};

std::shared_ptr<prng> CreatePrng();

#endif //JEOKERNEL_PRNG_H
