//
// Created by sigsegv on 7/6/21.
//

#include <stdlib.h>
#include <ctype.h>

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *nothing = nptr;
    while (isspace(*nptr)) {
        ++nptr;
    }
    int negative = 0;
    if (*nptr == '+') {
        ++nptr;
    } else if (*nptr == '-') {
        negative = 1;
        ++nptr;
    }
    while (isspace(*nptr)) {
        ++nptr;
    }
    if (base == 0 || base == 16) {
        if (nptr[0] == '0' && nptr[1] == 'x') {
            base = 16;
            nptr += 2;
        }
    }
    int valid = 0;
    if (base == 0 && *nptr == '0') {
        base = 8;
        ++nptr;
        valid = 1;
    }
    if (base == 0) {
        base = 10;
    }
    unsigned long num = 0;
    if (base <= 10) {
        while (1) {
            char c = *nptr;
            unsigned long digit = c - '0';
            if (c == '\0' || c < '0' || c > '9' || digit >= base) {
                if (valid) {
                    *endptr = (char *) nptr;
                    if (negative) {
                        num = 0 - num;
                    }
                    return num;
                } else {
                    *endptr = (char *) nothing;
                    return num;
                }
            }
            ++nptr;
            valid = 1;
            num = num * base;
            num += digit;
        }
    } else {
        while (1) {
            char c = *nptr;
            unsigned long digit = c - '0';
            int found = 0;
            if (c >= 'A' && c <= 'Z') {
                digit = c - 'A' + 10;
                found = 1;
            } else if (c >= 'a' && c <= 'z') {
                digit = c - 'a' + 10;
                found = 1;
            } else if (c >= '0' && c <= '9') {
                found = 1;
            }
            if (found) {
                valid = 1;
            }
            if (!found || digit >= base) {
                if (valid) {
                    *endptr = (char *) nptr;
                    if (negative) {
                        num = 0 - num;
                    }
                    return num;
                } else {
                    *endptr = (char *) nothing;
                    return num;
                }
            }
            ++nptr;
            num = num * base;
            num += digit;
        }
    }
}