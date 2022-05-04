//
// Created by sigsegv on 3/6/22.
//

#ifndef JEOKERNEL_SCSIVARIABLEDATA_H
#define JEOKERNEL_SCSIVARIABLEDATA_H

#include <cstdint>

class scsivariabledata {
private:
    uint8_t initialRead;
public:
    constexpr scsivariabledata(uint8_t initialRead) noexcept : initialRead(initialRead) {}
    virtual ~scsivariabledata() {}
    constexpr std::size_t InitialRead(std::size_t maxLength) noexcept {
        if (initialRead == 0 || maxLength < initialRead) {
            return maxLength;
        }
        return initialRead;
    }
    constexpr bool IsVariable() noexcept {
        return initialRead != 0;
    }
    virtual std::unique_ptr<scsivariabledata> clone() const = 0;
    virtual std::size_t TotalSize(const void *ptr, std::size_t initialRead, std::size_t maxLength) const = 0;
    std::size_t Remaining(const void *ptr, std::size_t initialRead, std::size_t maxLength) const {
        auto totalRead = TotalSize(ptr, initialRead, maxLength);
        if (totalRead > initialRead) {
            return totalRead - initialRead;
        } else {
            return 0;
        }
    }
};

class scsivariabledata_fixed : public scsivariabledata {
public:
    scsivariabledata_fixed() : scsivariabledata(0) {}
    std::unique_ptr<scsivariabledata> clone() const override {
        return std::unique_ptr<scsivariabledata>(new scsivariabledata_fixed());
    }
    std::size_t TotalSize(const void *ptr, std::size_t initialRead, std::size_t maxLength) const override {
        return maxLength;
    }
};

#endif //JEOKERNEL_SCSIVARIABLEDATA_H
