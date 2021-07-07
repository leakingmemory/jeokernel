//
// Created by sigsegv on 7/5/21.
//

#include <string.h>

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (str == ((char *) 0)) {
        str = *saveptr;
        if (str == ((char *) 0)) {
            return str;
        }
    }
    while (1) {
        int cont = 0;
        for (size_t i = 0; delim[i] != '\0'; i++) {
            if (delim[i] == *str) {
                cont = 1;
                break;
            }
        }
        if (!cont) {
            break;
        }
        ++str;
    }
    size_t i = 0;
    while (1) {
        char c = str[i];
        if (c != '\0') {
            for (size_t i = 0; delim[i] != '\0'; i++) {
                if (c == delim[i]) {
                    str[i] = '\0';
                    *saveptr = str + i + 1;
                    return str;
                }
            }
        } else {
            *saveptr = (char *) 0;
            return str;
        }
    }
}

static char *saveptr = (char *) 0;

char *strtok(char *str, const char *delim) {
    return strtok_r(str, delim, &saveptr);
}