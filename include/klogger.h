//
// Created by sigsegv on 19.04.2021.
//

#ifndef JEOKERNEL_KLOGGER_H
#define JEOKERNEL_KLOGGER_H

#include <stdint.h>

class KLogger {
public:
    virtual ~KLogger() {}

    virtual void print_at(uint8_t col, uint8_t row, const char *str) {
    }

    virtual void erase(int backtrack, int erase) {
    }

    virtual KLogger & operator << (const char *str) {
        return *this;
    };
    KLogger &print_u8(uint8_t bnum) {
        uint8_t ch1 = bnum >> 4;
        uint8_t ch2 = bnum & 15;
        ch1 += ch1 < 10 ? '0' : 'A' - 10;
        ch2 += ch2 < 10 ? '0' : 'A' - 10;
        char str[3]{(char) ch1, (char) ch2, 0};
        return *this << str;
    }
    KLogger &print_u16(uint16_t wnum) {
        return (this->print_u8(wnum >> 8)).print_u8(wnum & 0xff);
    }
    KLogger &print_u32(uint32_t wnum) {
        return (this->print_u16(wnum >> 16)).print_u16(wnum & 0xffff);
    }
    KLogger &print_u64(uint64_t qnum) {
        return (this->print_u32(qnum >> 32)).print_u32(qnum & 0xFFFFFFFF);
    }
    KLogger & operator << (uint8_t num) {
        return print_u8(num);
    }
    KLogger & operator << (uint16_t num) {
        return print_u16(num);
    }
    KLogger & operator << (uint32_t num) {
        return print_u32(num);
    }
    KLogger & operator << (uint64_t num) {
        return print_u64(num);
    }
};

void set_klogger(KLogger *klogger);
KLogger &get_klogger();
void wild_panic(const char *str);

#endif //JEOKERNEL_KLOGGER_H
