//
// Created by sigsegv on 24.04.2021.
//

#include <strings.h>
#include <string.h>

template <class copy_type, class addr_type> void memmove_impl(void *dest, const void *src, size_t n) {
    addr_type src_addr = (addr_type) src;
    unsigned int src_alignment = sizeof(copy_type) - (src_addr % sizeof(copy_type));
    if (src_alignment == sizeof(copy_type)) {
        src_alignment = 0;
    }
    uint8_t *dest_ptr = (uint8_t *) dest;
    const uint8_t *src_ptr = (uint8_t *) src;
    const uint8_t *src_end = &(src_ptr[n]);
    {
        unsigned int i = 0;
        while (i < src_alignment) {
            *dest_ptr = *src_ptr;
            ++dest_ptr;
            ++src_ptr;
            ++i;
        }
    }
    addr_type dst_addr = (addr_type) dest_ptr;
    unsigned int dst_alignment = sizeof(copy_type) - (dst_addr % sizeof(copy_type));
    if (dst_alignment == sizeof(copy_type)) {
        dst_alignment = 0;
    }
    {
        unsigned int i = 0;
        while (i < dst_alignment) {
            *dest_ptr = src_ptr[i];
            ++dest_ptr;
            ++i;
        }
    }
    copy_type *src_opt = (copy_type *) (void *) src_ptr;
    copy_type *dst_opt = (copy_type *) (void *) dest_ptr;
    copy_type *src_opt_end = (copy_type *) (((addr_type) src_end) - (((addr_type) src_end) % sizeof(copy_type)));
    if (src_opt < src_opt_end) {
        copy_type v1 = *src_opt;
        ++src_opt;
        unsigned int right_shift = dst_alignment << 3;
        unsigned int left_shift = (sizeof(copy_type) << 3) - right_shift;
        while (src_opt < src_opt_end) {
            copy_type v2 = *src_opt;
            ++src_opt;
            copy_type dst_ins = right_shift == (sizeof(copy_type) << 3) ? 0 : v1 >> right_shift;
            v1 = v2;
            v2 = left_shift != (sizeof(copy_type) << 3) ? v2 << left_shift : 0;
            dst_ins |= v2;
            *dst_opt = dst_ins;
            ++dst_opt;
        }
        dest_ptr = (uint8_t *) (void *) dst_opt;
        dst_addr = (addr_type) dest;
        src_ptr = ((uint8_t *) src) + (((addr_type) dest_ptr) - dst_addr);
        while (src_ptr < src_end) {
            *dest_ptr = *src_ptr;
            ++src_ptr;
            ++dest_ptr;
        }
    }
}

extern "C" {

    void *memmove(void *dest, const void *src, size_t n) {
        if (dest != src) {
            uint8_t *dest_ptr = (uint8_t *) dest;
            const uint8_t *src_ptr = (uint8_t *) src;
            const uint8_t *src_end = &(src_ptr[n]);
            if (dest_ptr < src_ptr || dest_ptr > src_end) {
                if (n > 32) {
                    memmove_impl<uint64_t,uint64_t>(dest, src, n);
                } else {
                    while (src_ptr < src_end) {
                        *dest_ptr = *src_ptr;
                        ++dest_ptr;
                        ++src_ptr;
                    }
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

    void *memset(void *ptr, int c, size_t n) {
        uint8_t *wr = (uint8_t *) ptr;
        for (size_t i = 0; i < n; i++) {
            wr[i] = c;
        }
        return ptr;
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