//
// Created by sigsegv on 19.04.2021.
//

#include <cpuio.h>
#include "b8000.h"

b8000::b8000() {
    videobuf = ((_plainvga_letter *) 0xb8000);
    cpos = 0;
    for (int i = 0; i < 80*25; i++) {
        videobuf[i].chr = ' ';
        videobuf[i].fg_color = 7;
        videobuf[i].bg_color = 0;
    }
    cursor();
}

void b8000::cursor() {
    outportb(0x3D4, 0x0F);
    outportb(0x3D5, cpos & 0xFF);
    outportb(0x3D4, 0x0E);
    outportb(0x3D5, cpos >> 8);
}

b8000 &b8000::operator<<(const char *str) {
    while (*str) {
        switch (*str) {
            case '\n':
                lnbreak();
                break;
            case '\r': {
                    uint16_t col = cpos % 80;
                    cpos -= col;
                }
                break;
            case '\t': {
                    uint16_t col = cpos & 7;
                    col = 8 - col;
                    cpos += col;
                    if (cpos >= (80*25)) {
                        cpos -= 80;
                        scroll();
                    }
                    for (uint16_t i = 0; i < col; i++) {
                        videobuf[cpos - i - 1].fg_color = 7;
                        videobuf[cpos - i - 1].bg_color = 0;
                        videobuf[cpos - i - 1].chr = ' ';
                    }
                }
                break;
            default:
                videobuf[cpos].fg_color = 7;
                videobuf[cpos].bg_color = 0;
                videobuf[cpos].chr = *str;
                ++cpos;
                if (cpos >= (80*25)) {
                    cpos -= 80;
                    scroll();
                }
        }
        str++;
    }
    cursor();
    return *this;
}

void b8000::lnbreak() {
    uint16_t col = cpos % 80;
    cpos += 80 - col;
    if (cpos >= (80*25)) {
        cpos -= 80;
        scroll();
    }
}

void b8000::scroll() {
    for (int i = 80; i < (80*25); i++) {
        videobuf[i-80] = videobuf[i];
    }
}
