//
// Created by sigsegv on 8/4/22.
//

#include <cstring>
#include <sstream>
#include <endian.h>
#include <hash/sha2alg.h>

template <typename T> constexpr T rightrotate(T val, unsigned int rot){
    const unsigned int mask = (8 * sizeof(val)) - 1;
    return (val >> (rot & mask)) | (val << ((0-rot) & mask));
}

static_assert(rightrotate<uint16_t>(0x101, 1) == 0x8080);
static_assert(rightrotate<uint32_t>(0x10001, 1) == 0x80008000UL);
static_assert(rightrotate<uint64_t>(0x10001, 1) == 0x8000000000008000ULL);

constexpr uint32_t to_big(uint32_t v) {
    return big_endian<uint32_t>(v).Raw();
}

void sha2alg::Digest(const uint32_t data[16]) {
    for (int i = 0; i < 16; i++) {
        w[i] = to_big(data[i]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rightrotate(w[i-15], 7) ^ rightrotate(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rightrotate(w[i-2], 17) ^ rightrotate(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    auto a = hv[0];
    auto b = hv[1];
    auto c = hv[2];
    auto d = hv[3];
    auto e = hv[4];
    auto f = hv[5];
    auto g = hv[6];
    auto h = hv[7];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rightrotate(e, 6) ^ rightrotate(e, 11) ^ rightrotate(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = rightrotate(a, 2) ^ rightrotate(a, 13) ^ rightrotate(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    hv[0] += a;
    hv[1] += b;
    hv[2] += c;
    hv[3] += d;
    hv[4] += e;
    hv[5] += f;
    hv[6] += g;
    hv[7] += h;
}

void sha2alg::Consume(const uint32_t data[16]) {
    mlength_bits += 16*32;
    Digest(data);
}

void sha2alg::Final(const uint8_t *c_data, size_t len) {
    if (len > 64) {
        return;
    }
    uint8_t data[64];
    if (len != 0) {
        memcpy(data, c_data, len);
    }
    mlength_bits += 8*len;
    if (len <= (64-1-8)) {
        data[len] = 0x80;
        ++len;
        while (len < (64-8)) {
            data[len] = 0;
            ++len;
        }
        big_endian<uint64_t> flen{mlength_bits};
        uint64_t *rawlen = (uint64_t *) &(data[len]);
        *rawlen = flen.Raw();
        Digest((uint32_t *)data);
    } else {
        bool endInFirst = len < 64;
        if (endInFirst) {
            data[len] = 0x80;
            ++len;
            while (len < 64) {
                data[len] = 0;
            }
        }
        Digest((uint32_t *)data);
        memset(data, 0, 64-8);
        big_endian<uint64_t> flen{mlength_bits};
        uint64_t *rawlen = (uint64_t *) &(data[64-8]);
        *rawlen = flen.Raw();
        Digest((uint32_t *)data);
    }
}

void sha2alg::Result(result_type &b_data) {
    uint32_t *data = (uint32_t *) &(b_data[0]);
    for (int i = 0; i < 16; i++) {
        big_endian<uint32_t> be{hv[i]};
        data[i] = be.Raw();
    }
}

std::string sha2alg::Hex(const uint8_t *data, size_t len) {
    std::stringstream ss{};
    ss << std::hex;
    for (size_t i = 0; i < len; i++) {
        ss << (data[i] < 0x10 ? "0" : "") << data[i];
    }
    std::string str{ss.str()};
    return str;
}

sha256::sha256() : sha2alg() {
    round_constants_type rc{0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    hash_values_type hv{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(this->k, rc, sizeof(this->k));
    memcpy(this->hv, hv, sizeof(this->hv));
}

void sha256::Result(result_type &b_data) {
    uint32_t *data = (uint32_t *) &(b_data[0]);
    for (int i = 0; i < 8; i++) {
        big_endian<uint32_t> be{hv[i]};
        data[i] = be.Raw();
    }
}

std::string sha256::Hex() {
    uint8_t data[32];
    Result(data);
    return sha2alg::Hex(data, 32);
}