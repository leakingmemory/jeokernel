//
// Created by sigsegv on 7/5/21.
//

#include <string.h>

char *strcat(char *str, const char *src) {
    char *result = str;
    while (*str != '\0') {
        ++str;
    }
    while (*src != '\0') {
        *(str++) = *(src++);
    }
    *str = '\0';
    return result;
}

char *strncat(char *str, const char *src, size_t n) {
    char *result = str;
    while (*str != '\0') {
        ++str;
    }
    size_t i = 0;
    while (*src != '\0' && i < n) {
        *(str++) = *(src++);
        ++i;
    }
    *str = '\0';
    return result;
}
