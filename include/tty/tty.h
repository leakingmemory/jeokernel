//
// Created by sigsegv on 10/2/22.
//

#ifndef JEOKERNEL_TTY_H
#define JEOKERNEL_TTY_H

#include <cstdint>
#include <functional>

struct tty_ioctl_response {
    intptr_t result;
    bool async;
};

class tty {
public:
    tty();
    tty_ioctl_response ioctl(intptr_t cmd, intptr_t arg, std::function<void (intptr_t)> func);
};

#endif //JEOKERNEL_TTY_H
