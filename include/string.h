//
// Created by sigsegv on 24.04.2021.
//

#ifndef JEOKERNEL_STRING_H
#define JEOKERNEL_STRING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    void *memmove(void *dest, const void *src, size_t n);
    void *memcpy(void *dst, const void *src, size_t n);
    int strcmp(const char *str1, const char *str2);
    void *memset(void *ptr, int c, size_t n);
    size_t strlen(const char *str);
    char *strcpy(char *dst, const char *src);
    int memcmp(const void *p1, const void *p2, size_t n);
    char *strcat(char *str, const char *src);
    char *strtok(char *str, const char *delim);
    char *strchr(char *str, int c);
    char *strrchr(char *str, int c);
    int strncmp(const char *str1, const char *str2, size_t n);

#define strerror(errno) ("error (strerror not implemented)")

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_STRING_H
