//
// Created by sigsegv on 24.04.2021.
//

#include <strings.h>
#include <string.h>

extern "C" {

    void *memmove(void *dest, const void *src, size_t n) {
        if (dest != src) {
            uint8_t *dest_ptr = (uint8_t *) dest;
            const uint8_t *src_ptr = (uint8_t *) src;
            const uint8_t *src_end = &(src_ptr[n]);
            if (dest_ptr < src_ptr || dest_ptr > src_end) {
                while (src_ptr < src_end) {
                    *dest_ptr = *src_ptr;
                    ++dest_ptr;
                    ++src_ptr;
                }
            } else {
                uint8_t *dest_end = &(dest_ptr[n]);
                while (dest_end > dest_ptr) {
                    --dest_end;
                    --src_end;
                    *dest_end = *src_end;
                }
            }
        }
        return dest;
    }
    void bcopy(const void *src, void *dest, size_t n) {
        memmove(dest, src, n);
    }

    void bzero(void *ptr, size_t n) {
        uint8_t *wr = (uint8_t *) ptr;
        for (size_t i = 0; i < n; i++) {
            wr[i] = 0;
        }
    }

    void *memcpy(void *dst, const void *src, size_t n) {
        bcopy(src, dst, n);
        return dst;
    }

    int strcmp(const char *str1, const char *str2) {
        while (*str1 == *str2 && *str1 != 0) {
            ++str1;
            ++str2;
        }
        return *str1 - *str2;
    }

    size_t strlen(const char *str) {
        size_t len = 0;
        while (str[len] != 0) {
            if (++len == 0) {
                return len - 1;
            }
        }
        return len;
    }

};