//
// Created by sigsegv on 10/4/22.
//

#ifndef JEOKERNEL_TERMIOS_H
#define JEOKERNEL_TERMIOS_H

#include <cstdint>

typedef uint32_t tcflag_t;
typedef uint8_t cc_t;
typedef uint32_t speed_t;

struct termios {
    static constexpr int ncc = 8;

    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[ncc];
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#endif //JEOKERNEL_TERMIOS_H
