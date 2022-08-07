//
// Created by sigsegv on 8/7/22.
//

#include <stdio.h>

extern "C" {
    void random_print(unsigned int num1, unsigned int num2, unsigned int num3) {
        printf("Random numbers: %u %u %u\n", num1, num2, num3);
    }

    void sha2alg_print(const char *str) {
        printf("%s", str);
    }
}