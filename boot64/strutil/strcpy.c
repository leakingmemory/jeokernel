//
// Created by sigsegv on 7/5/21.
//

#include <string.h>

char *strcpy(char *dst, const char *src) {
    while (*src != '\0') {
        *(dst++) = *(src++);
    }
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    while (*src != '\0' && i < n) {
        *(dst++) = *(src++);
        ++i;
    }
}