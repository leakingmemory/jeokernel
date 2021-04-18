//
// Created by sigsegv on 17.04.2021.
//

#include <strs.h>

template <int n> void __hexstr(char (&str)[n], unsigned long long int flex) {
    for (int i = 0; i < n; i++) {
        uint8_t part = flex % 16;
        flex = flex >> 4;
        if (part < 10) {
            str[n - i - 1] = part + '0';
        } else {
            str[n - i - 1] = part + 'A' - 10;
        }
    }
}

void hexstr(char (&str)[8], uint32_t num) {
    __hexstr(str, num);
}
