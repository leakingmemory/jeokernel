//
// Created by sigsegv on 8/6/22.
//

#include <cstdio>

extern "C" {
    void sha2alg_print(const char *str) {
        printf("%s", str);
    }

    int sha2alg_test();
}

int main() {
    return sha2alg_test();
}