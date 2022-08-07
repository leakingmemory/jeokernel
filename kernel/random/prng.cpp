//
// Created by sigsegv on 8/7/22.
//

#include <hash/sha2alg.h>
#include "prng.h"

template <class T> class prng_impl : public prng {
public:
    typedef typename T::chunk_type chunk_type;
    typedef typename T::result_type result_type;
private:
    T hashf;
    result_type feedback;
public:
    prng_impl() : hashf(), feedback() {}
    virtual ~prng_impl() = default;
    void Seed(const void *buf, size_t len) override {
        chunk_type chunk{};
        uint8_t *c = &(chunk[0]);
        size_t remaining = sizeof(chunk);
        {
            const uint8_t *p = &(feedback[0]);
            for (size_t i = 0; i < sizeof(feedback); i++) {
                if (remaining <= 0) {
                    hashf.Consume(chunk);
                    c = &(chunk[0]);
                    remaining = sizeof(chunk);
                }
                *c = *p;
                ++p;
                ++c;
                --remaining;
            }
        }
        {
            const uint8_t *p = (const uint8_t *) buf;
            for (size_t i = 0; i < len; i++) {
                if (remaining <= 0) {
                    hashf.Consume(chunk);
                    c = &(chunk[0]);
                    remaining = sizeof(chunk);
                }
                *c = *p;
                ++p;
                ++c;
                --remaining;
            }
        }
        if (remaining > 0) {
            while (remaining > 0) {
                *c = 0;
                ++c;
                --remaining;
            }
            hashf.Consume(chunk);
        }
    }
    void Generate(void *buf, size_t len) override {
        if (len <= 0) {
            return;
        }
        hashf.Result(feedback);
        if (len > sizeof(feedback)) {
            memcpy(buf, &(feedback[0]), sizeof(feedback));
            buf = ((uint8_t *) buf) + sizeof(feedback);
            len -= sizeof(feedback);
        } else {
            memcpy(buf, &(feedback[0]), len);
            return;
        }
        while (len > 0) {
            Seed(nullptr, 0);
            hashf.Result(feedback);
            if (len > sizeof(feedback)) {
                memcpy(buf, &(feedback[0]), sizeof(feedback));
                buf = ((uint8_t *) buf) + sizeof(feedback);
                len -= sizeof(feedback);
            } else {
                memcpy(buf, &(feedback[0]), len);
                return;
            }
        }
    }
};

class prng_default : public prng_impl<sha256> {
public:
    prng_default() : prng_impl<sha256>() {}
};

std::shared_ptr<prng> CreatePrng() {
    return std::make_shared<prng_default>();
}