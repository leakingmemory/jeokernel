//
// Created by sigsegv on 8/7/22.
//

#include <cstdio>

extern "C" {
    int random_test();

    int main() {
        return random_test();
    }
}