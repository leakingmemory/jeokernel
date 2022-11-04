//
// Created by sigsegv on 11/3/22.
//

#ifndef JEOKERNEL_STD_LIMITS_H
#define JEOKERNEL_STD_LIMITS_H

namespace std {
    template <typename T> class numeric_limits {
    public:
        static constexpr T min() noexcept {
            T p = 0;
            T v = 1;
            --p;
            if (p > v) {
                return 0;
            }
            while (p < v) {
                p = v;
                v = v << 1;
            }
            return v;
        }
        static constexpr T max() noexcept {
            if constexpr(min() != 0) {
                auto v = min();
                ++v;
                v = 0 - v;
                return v;
            } else {
                T v = 1;
                v = v << ((sizeof(T)*8)-1);
                T add = v - 1;
                T f = v + add;
                return f;
            }
        }
    };
}

#include <cstdint>

static_assert(std::numeric_limits<int8_t>::min() == -128);
static_assert(std::numeric_limits<int16_t>::min() == -32768);
static_assert(std::numeric_limits<int32_t>::min() == -2147483648);
static_assert(std::numeric_limits<int64_t>::min() == 0LL-(9223372036854775808ULL));

static_assert(std::numeric_limits<int8_t>::max() == 127);
static_assert(std::numeric_limits<int16_t>::max() == 32767);
static_assert(std::numeric_limits<int32_t>::max() == 2147483647);
static_assert(std::numeric_limits<int64_t>::max() == 9223372036854775807LL);

static_assert(std::numeric_limits<uint8_t>::min() == 0);
static_assert(std::numeric_limits<uint16_t>::min() == 0);
static_assert(std::numeric_limits<uint32_t>::min() == 0);
static_assert(std::numeric_limits<uint64_t>::min() == 0);

static_assert(std::numeric_limits<uint8_t>::max() == 255);
static_assert(sizeof(uint16_t) == 2);
static_assert(std::numeric_limits<uint16_t>::max() == 65535UL);
static_assert(sizeof(uint32_t) == 4);
static_assert(std::numeric_limits<uint32_t>::max() == 4294967295UL);
static_assert(sizeof(uint64_t) == 8);
static_assert(std::numeric_limits<uint64_t>::max() == 18446744073709551615ULL);

#endif //JEOKERNEL_STD_LIMITS_H
