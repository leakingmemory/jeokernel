//
// Created by sigsegv on 8/7/22.
//

#include <concurrency/hw_spinlock.h>
#include <mutex>
#include <hash/random.h>
#include "prng.h"

static hw_spinlock mtx{};
static std::shared_ptr<prng> prng{};

void GetRandom(void *buf, size_t len) {
    std::lock_guard lock{mtx};
    if (!prng) {
        prng = CreatePrng();
    }
    prng->Seed(nullptr, 0);
    prng->Generate(buf, len);
}
