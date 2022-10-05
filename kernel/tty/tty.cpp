//
// Created by sigsegv on 9/22/22.
//

#include "ttyhandler.h"
#include <tty/tty.h>
#include <tty/ttyinit.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <iostream>
#include <termios.h>

tty::tty() {
}

intptr_t tty::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    switch (cmd) {
        case TCGETS:
            return ctx.Write(arg, sizeof(termios), [ctx] (void *ptr) mutable {
                termios *t = (termios *) ptr;
                t->c_iflag = 0;
                t->c_oflag = 0;
                t->c_cflag = 0;
                t->c_lflag = 0;
                t->c_line = 0;
                for (int i = 0; i < termios::ncc; i++) {
                    t->c_cc[i] = 0;
                }
                return ctx.Return(0);
            });
    }
    std::cout << "tty->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}

void InitTty() {
    ttyhandler::Create("tty");
}
