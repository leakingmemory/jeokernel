//
// Created by sigsegv on 1/29/22.
//

#include <keyboard/keyboard_en.h>

static const char *prim_codes = "1234567890\n\0\0\t -=[]\\\0;'`,./";
static const char *mod_codes = "!@#$%^&*()\n\0\0\t _+{}|\0:\"~<>?";

uint32_t keyboard_en_codepage::Translate(uint32_t keycode) {
    bool upper{false};
    bool modifier{false};
    if ((keycode & KEYBOARD_CODE_BIT_CAPSLOCK) != 0) {
        upper = true;
    }
    if ((keycode & (KEYBOARD_CODE_BIT_LSHIFT | KEYBOARD_CODE_BIT_RSHIFT)) != 0) {
        upper = !upper;
        modifier = true;
    }
    uint16_t code = (uint16_t) (keycode & KEYBOARD_CODE_MASK);
    if (code >= 4) {
        if (code < 0x1E) {
            return (upper ? ('A' - 4) : ('a' - 4)) + code;
        }
        if (code < 0x39) {
            return modifier ? mod_codes[code - 0x1E] : prim_codes[code - 0x1E];
        }
    }
    return 0;
}
