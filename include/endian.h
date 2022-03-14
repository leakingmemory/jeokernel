//
// Created by sigsegv on 3/3/22.
//

#ifndef JEOKERNEL_ENDIAN_H
#define JEOKERNEL_ENDIAN_H

#include <cstdint>

constexpr bool my_platform_is_little_endian() noexcept { return ('ABCD' == 0x41424344UL); }
constexpr bool my_platform_is_middle_endian() noexcept { return ('ABCD' == 0x42414443UL); }
constexpr bool my_platform_is_big_endian() noexcept { return ('ABCD' == 0x44434241UL); }

static_assert(my_platform_is_little_endian() ^ my_platform_is_middle_endian() ^ my_platform_is_big_endian());
static_assert(my_platform_is_little_endian() || my_platform_is_middle_endian() || my_platform_is_big_endian());

template <typename T> constexpr T reverse_word_order(T input) {
    if (sizeof(T) <= 2) {
        return input;
    } else {
        T output{0};
        for (int i = 0; i < (sizeof(T) / 2); i++) {
            output *= 0x10000;
            output += input % 0x10000;
            input /= 0x10000;
        }
        return output;
    }
}

static_assert(reverse_word_order('ABCD') == 'CDAB');
static_assert(reverse_word_order<uint64_t>(0x1234567887654321) == 0x4321876556781234);

template <typename T> constexpr T reverse_byte_order(T input) {
    if constexpr(sizeof(T) == 1) {
        return input;
    } else if constexpr(sizeof(T) == 2) {
        return ((input % 0x100) * 0x100) + (input / 0x100);
    } else {
        T output{0};
        for (int i = 0; i < (sizeof(T) / 2); i++) {
            output *= 0x10000;
            output += reverse_byte_order<uint16_t>(input % 0x10000);
            input /= 0x10000;
        }
        return output;
    }
}

static_assert(reverse_byte_order('A') == 'A');
static_assert(reverse_byte_order<uint16_t>(0x1234) == 0x3412);
static_assert(reverse_byte_order<uint32_t>('ABCD') == 'DCBA');
static_assert(reverse_byte_order<uint64_t>(0x1234567887654321) == 0x2143658778563412);


template <typename T> class big_endian {
private:
    T value;
public:
    constexpr big_endian() noexcept : value(0) {
    }
    constexpr big_endian(T value) noexcept : value() {
        assign(value);
    }
    constexpr T assign(const T &value) noexcept {
        if constexpr(sizeof(T) == 1 || my_platform_is_big_endian()) {
            this->value = value;
        } else if (my_platform_is_middle_endian()) {
            this->value = reverse_word_order<T>(value);
        } else {
            this->value = reverse_byte_order<T>(value);
        }
        return value;
    }
    constexpr T unpack() const noexcept {
        if constexpr(sizeof(T) == 1 || my_platform_is_big_endian()) {
            return value;
        } else if (my_platform_is_middle_endian()) {
            return reverse_word_order<T>(value);
        } else {
            return reverse_byte_order<T>(value);
        }
    }
    constexpr T operator = (const T &value) noexcept {
        return assign(value);
    }
    constexpr T operator = (T &&value) noexcept {
        return assign(value);
    }
    constexpr operator T() const noexcept {
        return unpack();
    }

    constexpr T Raw() const noexcept {
        return value;
    }
} __attribute__((__packed__));

static_assert(sizeof(char) == sizeof(big_endian<char>));
static_assert(sizeof(int) == sizeof(big_endian<int>));

static_assert(big_endian<uint8_t>(0x41).Raw() == 'A');
static_assert(big_endian<uint16_t>(0x4142) == 0x4142);
static_assert(!my_platform_is_little_endian() || big_endian<uint16_t>(0x4142).Raw() == 0x4241);
static_assert(!my_platform_is_little_endian() || big_endian<uint32_t>(0x41424344).Raw() == 0x44434241);

#endif //JEOKERNEL_ENDIAN_H
