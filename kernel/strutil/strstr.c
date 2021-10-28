//
// Created by sigsegv on 7/7/21.
//

#include <stdio.h>

char *strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') {
        return (char *) haystack;
    }
    while (*haystack != '\0') {
        if (*haystack == *needle) {
            const char *h = haystack + 1;
            const char *n = needle + 1;
            while (*n != '\0') {
                if (*h != *n) {
                    break;
                }
                ++h;
                ++n;
            }
            if (*n == '\0') {
                return (char *) haystack;
            }
        }
        ++haystack;
    }
    return (char *) 0;
}
