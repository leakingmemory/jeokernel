//
// Created by sigsegv on 7/6/21.
//

#include <string.h>

char *strchr(char *str, int c) {
    while (*str != '\0') {
        if (*str == c) {
            return str;
        }
    }
    return (char *) 0;
}

char *strrchr(char *str, int c) {
    char *ptr = (char *) 0;
    while (*str != '\0') {
        if (*str == c) {
            ptr = str;
        }
    }
    return ptr;
}
