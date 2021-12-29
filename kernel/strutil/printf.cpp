//
// Created by sigsegv on 12/28/21.
//

#include <stdio.h>
#include <klogger.h>

extern "C" {
#define TMP_BUFSIZE 1024
    int vprintf(const char *format, va_list args) {
        char str[TMP_BUFSIZE];
        int err = vsnprintf(str, TMP_BUFSIZE, format, args);
        if (err < 0) {
            get_klogger() << format;
            return err;
        }
        str[TMP_BUFSIZE - 1] = '\0';
        get_klogger() << str;
        return err;
    }

    int printf(const char *format, ...) {
        va_list list;
        va_start(list, format);
        int err = vprintf(format, list);
        va_end(list);
        return err;
    }
}