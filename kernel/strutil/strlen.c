//
// Created by sigsegv on 7/5/21.
//

#include <string.h>

size_t strnlen(const char *str, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen) {
        if (str[len] == '\0') {
            return len;
        }
        len++;
    }
    return len;
}
