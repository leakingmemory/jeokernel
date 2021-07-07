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
    size_t strnlen(const char *str, size_t maxlen);
    char *strcpy(char *dst, const char *src);
    char *strncpy(char *dst, const char *src, size_t n);
    int memcmp(const void *p1, const void *p2, size_t n);
    char *strcat(char *str, const char *src);
    char *strncat(char *str, const char *src, size_t n);
    char *strtok(char *str, const char *delim);
    char *strtok_r(char *str, const char *delim, char **saveptr);
    char *strchr(char *str, int c);
    char *strrchr(char *str, int c);
    int strncmp(const char *str1, const char *str2, size_t n);
    char *strstr(const char *haystack, const char *needle);

#define strerror(errno) ("error (strerror not implemented)")

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_STRING_H
