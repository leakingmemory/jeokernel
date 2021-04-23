//
// Created by sigsegv on 24.04.2021.
//

#ifndef JEOKERNEL_STRINGS_H
#define JEOKERNEL_STRINGS_H

#include <stdint.h>

extern "C" {
    void bcopy(const void *src, void *dest, size_t n);
    void bzero(void *ptr, size_t n);
};

#endif //JEOKERNEL_STRINGS_H
