//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>

extern "C" {

    void start_m64() {
        uint8_t *whoop = (uint8_t *) 0xB8000;
        whoop[0] = '-';
        whoop[1] = '-';
        whoop[2] = '-';
        whoop[3] = '-';

        while (1) {
        }
    }

}