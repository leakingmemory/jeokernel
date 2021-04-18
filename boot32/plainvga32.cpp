//
// Created by sigsegv on 17.04.2021.
//

#include "plainvga32.h"

static _plainvga32buf_text &get_vgabuf() {
    _plainvga32buf_text *vgabuf = (_plainvga32buf_text *) 0xB8000;
    return *vgabuf;
}

plainvga32::plainvga32() : vgabuf(get_vgabuf()) {
}

void plainvga32::display(uint8_t line, uint8_t start, const char *str) {
    uint16_t vector = ((uint16_t) line) * 80;
    vector += start;
    while (*str) {
        _plainvga_letter &letter = vgabuf.chmem[vector % 2000];
        letter.bg_color = 0;
        letter.fg_color = 7;
        letter.chr = *str;
        vector++;
        str++;
    }
}
