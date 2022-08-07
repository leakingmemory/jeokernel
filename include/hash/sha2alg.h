//
// Created by sigsegv on 8/4/22.
//

#ifndef JEOKERNEL_SHA2ALG_H
#define JEOKERNEL_SHA2ALG_H

#include <cstdint>
#include <string>

class sha2alg {
public:
    typedef uint32_t round_constants_type[64];
    typedef uint8_t chunk_type[64];
    typedef uint8_t result_type[64];
    typedef uint32_t hash_values_type[8];
private:
    uint32_t w[64];
protected:
    round_constants_type k;
    hash_values_type hv;
private:
    uint64_t mlength_bits;
protected:
    constexpr sha2alg() : w(), k(), hv(), mlength_bits(0) {}
private:
    void Digest(const uint32_t data[16]);
public:
    void Consume(const uint32_t data[16]);
    void Consume(const chunk_type data) {
        const uint32_t *d = (const uint32_t *) &(data[0]);
        Consume(d);
    }
    void Final(const uint8_t *data, size_t len);
    void Result(result_type &data);
protected:
    std::string Hex(const uint8_t *data, size_t len);
public:
    std::string Hex() {
        uint8_t data[64];
        Result(data);
        return Hex(data, 64);
    }
};

class sha256 : public sha2alg {
public:
    typedef uint8_t result_type[32];
    sha256();
    void Result(result_type &data);
    std::string Hex();
};


#endif //JEOKERNEL_SHA2ALG_H
