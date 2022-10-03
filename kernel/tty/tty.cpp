//
// Created by sigsegv on 9/22/22.
//

#include "ttyhandler.h"
#include <tty/tty.h>
#include <tty/ttyinit.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <iostream>

tty::tty() {
}

tty_ioctl_response tty::ioctl(intptr_t cmd, intptr_t arg, std::function<void (intptr_t)> func) {
    std::cout << "tty->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return {.result = EOPNOTSUPP, .async = false};
}

void InitTty() {
    ttyhandler::Create("tty");
}
