//
// Created by sigsegv on 19.04.2021.
//

#ifndef JEOKERNEL_B8000_H
#define JEOKERNEL_B8000_H

#include <stdint.h>

struct _plainvga_letter {
    uint8_t chr : 8;
    uint8_t fg_color : 4;
    uint8_t bg_color : 4;
} __attribute__((__packed__));

static_assert(sizeof(_plainvga_letter) == 2);

class b8000 {
private:
    _plainvga_letter *videobuf;
    uint16_t cpos; // Cursor
public:
    b8000();
    void lnbreak();
    void print_at(uint8_t col, uint8_t row, const char *str);
    void erase(int backtrack, int clear);
    b8000 & operator << (const char *str);

    b8000 &print_u8(uint8_t bnum) {
        uint8_t ch1 = bnum >> 4;
        uint8_t ch2 = bnum & 15;
        ch1 += ch1 < 10 ? '0' : 'A' - 10;
        ch2 += ch2 < 10 ? '0' : 'A' - 10;
        char str[3]{(char) ch1, (char) ch2, 0};
        return *this << str;
    }
    b8000 &print_u16(uint16_t wnum) {
        return (this->print_u8(wnum >> 8)).print_u8(wnum & 0xff);
    }
    b8000 &print_u32(uint32_t wnum) {
        return (this->print_u16(wnum >> 16)).print_u16(wnum & 0xffff);
    }
    b8000 &print_u64(uint64_t qnum) {
        return (this->print_u32(qnum >> 32)).print_u32(qnum & 0xFFFFFFFF);
    }
private:
    void cursor();
    void scroll();
};


#endif //JEOKERNEL_B8000_H
