//
// Created by sigsegv on 24.04.2021.
//

#ifndef JEOKERNEL_STRING_H
#define JEOKERNEL_STRING_H

#include <stdint.h>

extern "C" {
    void *memmove(void *dest, const void *src, size_t n);
    void *memcpy(void *dst, const void *src, size_t n);
    int strcmp(const char *str1, const char *str2);
    void *memset(void *ptr, int c, size_t n);
    size_t strlen(const char *str);
};

#endif //JEOKERNEL_STRING_H
