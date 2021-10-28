//
// Created by sigsegv on 7/5/21.
//

#include <string.h>

int strncmp(const char *str1, const char *str2, size_t n) {
    while (n > 0) {
        n--;
        char c1 = *(str1++);
        char c2 = *(str2++);
        if (c1 != c2) {
            return c1 - c2;
        }
        if (c1 == 0) {
            return 0;
        }
    }
    return 0;
}