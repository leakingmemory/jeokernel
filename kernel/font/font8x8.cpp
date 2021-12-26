//
// Created by sigsegv on 12/25/21.
//

#include <font/font8x8.h>
#include "font8x8/font8x8.h"

template <int n> uint64_t GetRaw8x8Character(char (&character)[n][8], uint32_t ch) {
    uint64_t *array = (uint64_t *) (void *) &(character[0][0]);
    return array[ch];
}

template <int n> uint64_t GetRaw8x8Character(uint8_t (&character)[n][8], uint32_t ch) {
    uint64_t *array = (uint64_t *) (void *) &(character[0][0]);
    return array[ch];
}

uint64_t Get8x8Character(uint32_t ch) {
    if (ch < 0x80) {
        return GetRaw8x8Character(font8x8_basic, ch);
    } else if (ch < 0xA0) {
        return GetRaw8x8Character(font8x8_control, ch - 0x80);
    } else if (ch < 0x100) {
        return GetRaw8x8Character(font8x8_ext_latin, ch - 0xA0);
    } else if (ch <= 0x3c9) {
        if (ch >= 0x390) {
            return GetRaw8x8Character(font8x8_greek, ch - 0x390);
        } else if (ch == 0x192) {
            return GetRaw8x8Character(font8x8_misc, 1);
        }
    } else if (ch <= 0x257F) {
        if (ch >= 0x2500) {
            return GetRaw8x8Character(font8x8_box, ch - 0x2500);
        } else if (ch == 0x20A7) {
            return GetRaw8x8Character(font8x8_misc, 0);
        } else if (ch == 0x2310) {
            return GetRaw8x8Character(font8x8_misc, 4);
        }
    } else if (ch <= 0x259F) {
        return GetRaw8x8Character(font8x8_block, ch - 0x2580);
    } else if (ch <= 0x309F) {
        if (ch >= 0x3040) {
            return GetRaw8x8Character(font8x8_hiragana, ch - 0x3040);
        }
    } else if (ch <= 0xE55A) {
        if (ch >= 0xE541) {
            return GetRaw8x8Character(font8x8_sga, ch - 0xE541);
        }
    }
    return 0;
}