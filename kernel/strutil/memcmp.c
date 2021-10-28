//
// Created by sigsegv on 7/6/21.
//

#include <string.h>

int memcmp(const void *p1, const void *p2, size_t n) {
    uint8_t *u1, *u2;
    u1 = (uint8_t *) p1;
    u2 = (uint8_t *) p2;
    while (n > 0) {
        int32_t v1 = *(u1++);
        int32_t v2 = *(u2++);
        --n;
        int32_t diff = v1 - v2;
        if (diff != 0) {
            return diff;
        }
    }
}

