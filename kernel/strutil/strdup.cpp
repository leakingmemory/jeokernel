//
// Created by sigsegv on 4/28/22.
//

#include <string.h>
#include <stdlib.h>

extern "C" {
    char *strdup(const char *str) {
        auto len = strlen(str) + 1;
        char *buf = (char *) malloc(len);
        strncpy(buf, str, len);
        return buf;
    }
}
