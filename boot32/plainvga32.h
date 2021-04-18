//
// Created by sigsegv on 17.04.2021.
//

#ifndef JEOKERNEL_PLAINVGA32_H
#define JEOKERNEL_PLAINVGA32_H

#include <stdint.h>

struct _plainvga_letter {
    uint8_t chr : 8;
    uint8_t fg_color : 4;
    uint8_t bg_color : 4;
};

static_assert(sizeof(_plainvga_letter) == 2);

struct _plainvga32buf_text {
    _plainvga_letter chmem[2000];
};

static_assert(sizeof(_plainvga32buf_text) == 4000);

class plainvga32 {
private:
    _plainvga32buf_text &vgabuf;
public:
    plainvga32();
    void display(uint8_t line, uint8_t start, const char *str);
};


#endif //JEOKERNEL_PLAINVGA32_H
