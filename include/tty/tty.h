//
// Created by sigsegv on 10/2/22.
//

#ifndef JEOKERNEL_TTY_H
#define JEOKERNEL_TTY_H

#include <cstdint>
#include <functional>
#include <exec/callctx.h>

class tty {
public:
    tty();
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg);
};

#endif //JEOKERNEL_TTY_H
