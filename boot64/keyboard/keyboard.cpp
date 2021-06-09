
#include <keyboard/keyboard.h>

void keyboard_type2_state_machine::raw_code(uint8_t ch) {
    bool ignore{false};
    if (ignore_length > 0) {
        if (ignore_codes[0] == ch) {
            ignore = true;
            --ignore_length;
            for (int i = 0; i < ignore_length; i++) {
                ignore_codes[i] = ignore_codes[i + 1];
            }
        }
    }
    if (!ignore) {
        uint16_t keycode{0};
        if (scrolllock) {
            keycode |= KEYBOARD_CODE_BIT_SCROLLLOCK;
        }
        if (capslock) {
            keycode |= KEYBOARD_CODE_BIT_CAPSLOCK;
        }
        if (numlock) {
            keycode |= KEYBOARD_CODE_BIT_NUMLOCK;
        }
        switch (ch) {
            case 0x58: {
                /* caps lock */
                if (capslock) {
                    if (recorded_length == 0) {
                        recorded_codes[0] = ch;
                        recorded_length = 1;
                    } else if (recorded_length == 2 && recorded_codes[0] == ch && recorded_codes[1] == 0xF0) {
                        SetLeds(false, scrolllock, numlock);
                        recorded_length = 0;
                    } else {
                        recorded_length = 0;
                    }
                } else {
                    SetLeds(true, scrolllock, numlock);
                    ignore_codes[0] = 0xF0;
                    ignore_codes[1] = ch;
                    ignore_length = 2;
                }
            }
                break;
            case 0x7E: {
                /* scroll lock */
                if (scrolllock) {
                    if (recorded_length == 0) {
                        recorded_codes[0] = ch;
                        recorded_length = 1;
                    } else if (recorded_length == 2 && recorded_codes[0] == ch && recorded_codes[1] == 0xF0) {
                        SetLeds(capslock, false, numlock);
                        recorded_length = 0;
                    } else {
                        recorded_length = 0;
                    }
                } else {
                    SetLeds(capslock, true, numlock);
                    ignore_codes[0] = 0xF0;
                    ignore_codes[1] = ch;
                    ignore_length = 2;
                }
            }
                break;
            case 0x77: {
                /* num lock */
                if (recorded_length == 2 && recorded_codes[0] == 0xE1 && recorded_codes[1] == 0x14) {
                    recorded_codes[recorded_length++] = ch;
                } else if (
                        recorded_length == 7 &&
                        recorded_codes[0] == 0xE1 &&
                        recorded_codes[1] == 0x14 &&
                        recorded_codes[2] == 0x77 &&
                        recorded_codes[3] == 0xE1 &&
                        recorded_codes[4] == 0xF0 &&
                        recorded_codes[5] == 0x14 &&
                        recorded_codes[6] == 0xF0
                        ) {
                    recorded_length = 0;
                    keycode |= KEYBOARD_CODE_PAUSE;
                    this->layer2_keycode(keycode);
                } else if (numlock) {
                    if (recorded_length == 0) {
                        recorded_codes[0] = ch;
                        recorded_length = 1;
                    } else if (recorded_length == 2 && recorded_codes[0] == ch && recorded_codes[1] == 0xF0) {
                        SetLeds(capslock, scrolllock, false);
                        recorded_length = 0;
                    } else {
                        recorded_length = 0;
                    }
                } else {
                    SetLeds(capslock, scrolllock, true);
                    ignore_codes[0] = 0xF0;
                    ignore_codes[1] = ch;
                    ignore_length = 2;
                }
            }
                break;
            case 0xF0: {
                if (recorded_length < REC_BUFFER_SIZE) {
                    recorded_codes[recorded_length++] = ch;
                }
            }
                break;
            case 0xE0: {
                if (recorded_length < REC_BUFFER_SIZE) {
                    recorded_codes[recorded_length++] = ch;
                }
            }
                break;
            default:
                bool swallow{false};
                if (recorded_length == 0 && ch == 0xE1) {
                    recorded_codes[0] = ch;
                    recorded_length = 1;
                    swallow = true;
                } else if (recorded_length == 1) {
                    recorded_length = 0;
                    if (recorded_codes[0] == 0xE0) {
                        if (ch == 0x12) {
                            recorded_codes[1] = 0x12;
                            recorded_length = 2;
                            swallow = true;
                        } else {
                            keycode |= KEYBOARD_CODE_EXTENDED;
                            keycode += ch;
                        }
                    } else if (recorded_codes[0] == 0xF0) {
                        keycode |= KEYBOARD_CODE_BIT_RELEASE;
                        keycode += ch;
                    } else if (recorded_codes[0] == 0xE1 && ch == 0x14) {
                        recorded_codes[1] = 0x14;
                        recorded_length = 2;
                        swallow = true;
                    } else {
                        keycode += ch;
                    }
                } else if (recorded_length == 2) {
                    recorded_length = 0;
                    if (recorded_codes[0] == 0xE0 && recorded_codes[1] == 0xF0) {
                        if (ch == 0x7C) {
                            recorded_codes[2] = ch;
                            recorded_length = 3;
                            swallow = true;
                        } else {
                            keycode |= KEYBOARD_CODE_EXTENDED | KEYBOARD_CODE_BIT_RELEASE;
                            keycode += ch;
                        }
                    } else {
                        keycode += ch;
                    }
                } else if (recorded_length == 3) {
                    recorded_length = 0;
                    if (recorded_codes[0] == 0xE0 && recorded_codes[1] == 0x12 && recorded_codes[2] == 0xE0 &&
                        ch == 0x7C) {
                        keycode += KEYBOARD_CODE_PRINTSCREEN;
                    } else if (recorded_codes[0] == 0xE1 && recorded_codes[1] == 0x14 && recorded_codes[2] == 0x77 && ch == 0xE1) {
                        recorded_codes[3] = 0xE1;
                        recorded_length = 4;
                        swallow = true;
                    } else {
                        keycode += ch;
                    }
                } else if (recorded_length == 5) {
                    recorded_length = 0;
                    if (
                            recorded_codes[0] == 0xE0 &&
                            recorded_codes[1] == 0xF0 &&
                            recorded_codes[2] == 0x7C &&
                            recorded_codes[3] == 0xE0 &&
                            recorded_codes[4] == 0xF0 &&
                            ch == 0x12
                            ) {
                        keycode |= KEYBOARD_CODE_BIT_RELEASE | KEYBOARD_CODE_PRINTSCREEN;
                    } else if (
                            recorded_codes[0] == 0xE1 &&
                            recorded_codes[1] == 0x14 &&
                            recorded_codes[2] == 0x77 &&
                            recorded_codes[3] == 0xE1 &&
                            recorded_codes[4] == 0xF0 &&
                            ch == 0x14
                            ) {
                        recorded_codes[5] = 0x14;
                        recorded_length = 6;
                        swallow = true;
                    } else {
                        keycode += ch;
                    }
                } else {
                    recorded_length = 0;
                    keycode += ch;
                }
                if (!swallow) {
                    this->layer2_keycode(keycode);
                }
        }
    }
}

void keyboard_type2_state_machine::SetLeds(bool capslock, bool scrolllock, bool numlock) {
    this->capslock = capslock;
    this->scrolllock = scrolllock;
    this->numlock = numlock;
}

void keyboard_type2_state_machine::layer2_keycode(uint16_t code) {
    uint16_t key = code & KEYBOARD_CODE_MASK;
    uint16_t bit{0};
    switch (key) {
        case 0x14: {
            bit = KEYBOARD_CODE_BIT_LCONTROL;
        } break;
        case 0x114: {
            bit = KEYBOARD_CODE_BIT_RCONTROL;
        } break;
    }
    if (bit != 0) {
        if ((code & KEYBOARD_CODE_BIT_RELEASE) == 0) {
            state |= bit;
        } else {
            state &= ~bit;
        }
    }
    this->keycode(((uint32_t) code) | state);
}
