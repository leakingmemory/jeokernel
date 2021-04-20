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
    b8000 & operator << (const char *str);
private:
    void cursor();
    void scroll();
};


#endif //JEOKERNEL_B8000_H
