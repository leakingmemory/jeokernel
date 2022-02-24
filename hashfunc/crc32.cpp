//
// Created by sigsegv on 2/23/22.
//

#include <cstdint>
#include <hashfunc/crc32.h>

constexpr uint32_t crc32_value(uint32_t crcnum) noexcept {
    uint32_t result{0};
    for (int i = 0; i < 8; i++) {
        bool bit{((crcnum ^ result) & 1) != 0};
        result = result >> 1;
        if (bit) {
            result = result ^ 0xEDB88320;
        }
        crcnum = crcnum >> 1;
    }
    return result;
}

struct crc32table {
private:
    uint32_t values[256];
public:
    constexpr crc32table() noexcept : values{} {
        for (uint32_t i = 0; i < 256; i++) {
            values[i] = crc32_value(i);
        }
    }

    uint32_t operator [](uint8_t index) const {
        return values[index];
    }
};

constexpr uint32_t crc32_tablevalue_idx(uint32_t crc, uint8_t byte) noexcept {
    return (crc & 0xFF) ^ byte;
}

constexpr uint32_t crc32_iteration(uint32_t crc, uint32_t tablevalue, uint8_t byte) noexcept {
    uint32_t nextcrc{tablevalue ^ (crc / 256)};
    return nextcrc;
}

constexpr uint32_t crc32_initialize() noexcept {
    return 0xFFFFFFFF;
}

constexpr uint32_t crc32_finalize(uint32_t crc) noexcept {
    return crc ^ 0xFFFFFFFF;
}

const crc32table crcvalues{};

crc32::crc32() {
    state = crc32_initialize();
}

void crc32::encode(uint8_t byte) {
    uint32_t tblidx{crc32_tablevalue_idx(state, byte)};
    state = crc32_iteration(state, crcvalues[tblidx], byte);
}

uint32_t crc32::complete() {
    return crc32_finalize(state);
}


/* Begin test */

template <int n> constexpr uint32_t crc32_algorithm(const uint8_t data[n]) noexcept {
    uint32_t crc = crc32_initialize();
    for (std::size_t i = 0; i < n; i++) {
        uint32_t tblidx{crc32_tablevalue_idx(crc, data[i])};
        crc = crc32_iteration(crc, crc32_value(tblidx), data[i]);
    }
    return crc32_finalize(crc);
}

constexpr uint8_t string_123456789[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

static_assert(crc32_algorithm<9>(string_123456789) == 0xCBF43926);

/* End test */
