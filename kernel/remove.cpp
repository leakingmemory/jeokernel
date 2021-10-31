//
// Created by sigsegv on 10/31/21.
//

#include <stdio.h>

extern "C" {
    int remove(const char *pathname) {
        return -1;
    }
}
