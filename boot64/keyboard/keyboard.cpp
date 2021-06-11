
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
        uint32_t keycode{0};
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

void keyboard_type2_state_machine::layer2_keycode(uint32_t code) {
    uint16_t key = (uint16_t) (code & KEYBOARD_CODE_MASK);
    uint32_t bit{0};
    switch (key) {
        case 0x11: {
            bit = KEYBOARD_CODE_BIT_LALT;
        } break;
        case 0x12: {
            bit = KEYBOARD_CODE_BIT_LSHIFT;
        } break;
        case 0x14: {
            bit = KEYBOARD_CODE_BIT_LCONTROL;
        } break;
        case 0x59: {
            bit = KEYBOARD_CODE_BIT_RSHIFT;
        } break;
        case 0x111: {
            bit = KEYBOARD_CODE_BIT_RALT;
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

const static uint16_t usbcodes[] = {
        /*00*/ 0 /* none */,
        /*01*/ 0,
        /*02*/ 0xFC,
        /*03*/ 0 /* unassigned */,
        /*04*/ 0x1C,
        /*05*/ 0x32,
        /*06*/ 0x21,
        /*07*/ 0x23,
        /*08*/ 0x24,
        /*09*/ 0x2B,
        /*0A*/ 0x34,
        /*0B*/ 0x33,
        /*0C*/ 0x43,
        /*0D*/ 0x3B,
        /*0E*/ 0x42,
        /*0F*/ 0x4B,
        /*10*/ 0x3A,
        /*11*/ 0x31,
        /*12*/ 0x44,
        /*13*/ 0x4D,
        /*14*/ 0x15,
        /*15*/ 0x2D,
        /*16*/ 0x1B,
        /*17*/ 0x2C,
        /*18*/ 0x3C,
        /*19*/ 0x2A,
        /*1A*/ 0x1D,
        /*1B*/ 0x22,
        /*1C*/ 0x35,
        /*1D*/ 0x1A,
        /*1E*/ 0x16,
        /*1F*/ 0x1E,
        /*20*/ 0x26,
        /*21*/ 0x25,
        /*22*/ 0x2E,
        /*23*/ 0x36,
        /*24*/ 0x3D,
        /*25*/ 0x3E,
        /*26*/ 0x46,
        /*27*/ 0x45,
        /*28*/ 0x5A,
        /*29*/ 0x76,
        /*2A*/ 0x66,
        /*2B*/ 0x0D,
        /*2C*/ 0x29,
        /*2D*/ 0x4E,
        /*2E*/ 0x55,
        /*2F*/ 0x54,
        /*30*/ 0x5B,
        /*31*/ 0x5D,
        /*32*/ 0x5D,
        /*33*/ 0x4C,
        /*34*/ 0x52,
        /*35*/ 0x0E,
        /*36*/ 0x41,
        /*37*/ 0x49,
        /*38*/ 0x4A,
        /*39*/ 0x58,
        /*3A*/ 0x05,
        /*3B*/ 0x06,
        /*3C*/ 0x04,
        /*3D*/ 0x0C,
        /*3E*/ 0x03,
        /*3F*/ 0x0B,
        /*40*/ 0x83,
        /*41*/ 0x0A,
        /*42*/ 0x01,
        /*43*/ 0x09,
        /*44*/ 0x78,
        /*45*/ 0x07,
        /*46*/ 0x17C,
        /*47*/ 0x7E,
        /*48*/ (KEYBOARD_CODE_PAUSE & KEYBOARD_CODE_MASK),
        /*49*/ 0x170 /*insert*/,
        /*4A*/ 0x16C /*home*/,
        /*4B*/ 0x17D /*page up*/,
        /*4C*/ 0x171 /*delete*/,
        /*4D*/ 0x169 /*end*/,
        /*4E*/ 0x17A /*page down*/,
        /*4F*/ 0x174 /*right*/,
        /*50*/ 0x16B /*left*/,
        /*51*/ 0x172 /*down*/,
        /*52*/ 0x175 /*up*/,
        /*53*/ 0 /*NONE*/,
        /*54*/ 0x14A /*KP /*/,
        /*55*/ 0x7C /*KP **/,
        /*56*/ 0x7B /*KP -*/,
        /*57*/ 0x79 /*KP +*/,
        /*58*/ 0x15A /*KP enter*/,
        /*59*/ 0x69 /*KP 1*/,
        /*5A*/ 0x72 /*KP 2*/,
        /*5B*/ 0x7A /*KP 3*/,
        /*5C*/ 0x6B /*KP 4*/,
        /*5D*/ 0x73 /*KP 5*/,
        /*5E*/ 0x74 /*KP 6*/,
        /*5F*/ 0x6C /*KP 7*/,
        /*60*/ 0x75 /*KP 8*/,
        /*61*/ 0x7D /*KP 9*/,
        /*62*/ 0x70 /*KP 0*/,
        /*63*/ 0x71 /*KP .*/,
        /*64*/ 0 /*NONE*/,
        /*65*/ 0 /*NONE*/,
        /*66*/ 0 /*NONE*/,
        /*67*/ 0x55 /*KP =*/,
        /*68*/ 0 /*F13*/,
        /*69*/ 0 /*F14*/,
        /*6A*/ 0 /*F15*/,
        /*6B*/ 0 /*F16*/,
        /*6C*/ 0 /*F17*/,
        /*6D*/ 0 /*F18*/,
        /*6E*/ 0 /*F19*/,
        /*6F*/ 0 /*F20*/,
        /*70*/ 0 /*F21*/,
        /*71*/ 0 /*F22*/,
        /*72*/ 0 /*F23*/,
        /*73*/ 0 /*F24*/,
        /*74*/ 0 /*NONE*/,
        /*75*/ 0 /*NONE*/,
        /*76*/ 0 /*NONE*/,
        /*77*/ 0 /*NONE*/,
        /*78*/ 0 /*NONE*/,
        /*79*/ 0 /*NONE*/,
        /*7A*/ 0 /*NONE*/,
        /*7B*/ 0 /*NONE*/,
        /*7C*/ 0 /*NONE*/,
        /*7D*/ 0 /*NONE*/,
        /*7E*/ 0 /*NONE*/,
        /*7F*/ 0 /*NONE*/,
        /*80*/ 0 /*nonE*/,
        /*81*/ 0x137,
        /*82*/ 0x13F,
        /*83*/ 0x15E,
        /*84*/ 0,
        /*85*/ 0 /*KP ,*/,
        /*86*/ 0x55 /*KP =*/,
        /*87*/ 0 /*NONE*/,
        /*88*/ 0 /*NONE*/,
        /*89*/ 0 /*NONE*/,
        /*8A*/ 0 /*NONE*/,
        /*8B*/ 0 /*NONE*/,
        /*8C*/ 0 /*NONE*/,
        /*8D*/ 0 /*NONE*/,
        /*8E*/ 0 /*NONE*/,
        /*8F*/ 0 /*NONE*/,
        /*90*/ 0 /*none*/,
        /*91*/ 0 /*none*/,
        /*92*/ 0 /*none*/,
        /*93*/ 0 /*none*/,
        /*94*/ 0 /*NONE*/,
        /*95*/ 0 /*NONE*/,
        /*96*/ 0 /*NONE*/,
        /*97*/ 0 /*NONE*/,
        /*98*/ 0 /*NONE*/,
        /*99*/ 0 /*NONE*/,
        /*9A*/ 0 /*NONE*/,
        /*9B*/ 0 /*NONE*/,
        /*9C*/ 0 /*NONE*/,
        /*9D*/ 0 /*NONE*/,
        /*9E*/ 0 /*NONE*/,
        /*9F*/ 0 /*NONE*/,
        /*A0*/ 0 /*none*/,
        /*A1*/ 0 /*none*/,
        /*A2*/ 0 /*none*/,
        /*A3*/ 0 /*none*/,
        /*A4*/ 0 /*NONE*/,
        /*A5*/ 0 /*NONE*/,
        /*A6*/ 0 /*NONE*/,
        /*A7*/ 0 /*NONE*/,
        /*A8*/ 0 /*NONE*/,
        /*A9*/ 0 /*NONE*/,
        /*AA*/ 0 /*NONE*/,
        /*AB*/ 0 /*NONE*/,
        /*AC*/ 0 /*NONE*/,
        /*AD*/ 0 /*NONE*/,
        /*AE*/ 0 /*NONE*/,
        /*AF*/ 0 /*NONE*/,
        /*B0*/ 0 /*none*/,
        /*B1*/ 0 /*none*/,
        /*B2*/ 0 /*none*/,
        /*B3*/ 0 /*none*/,
        /*B4*/ 0 /*NONE*/,
        /*B5*/ 0 /*NONE*/,
        /*B6*/ 0 /*NONE*/,
        /*B7*/ 0 /*NONE*/,
        /*B8*/ 0 /*NONE*/,
        /*B9*/ 0 /*NONE*/,
        /*BA*/ 0 /*NONE*/,
        /*BB*/ 0 /*NONE*/,
        /*BC*/ 0 /*NONE*/,
        /*BD*/ 0 /*NONE*/,
        /*BE*/ 0 /*NONE*/,
        /*BF*/ 0 /*NONE*/,
        /*C0*/ 0 /*none*/,
        /*C1*/ 0 /*none*/,
        /*C2*/ 0 /*none*/,
        /*C3*/ 0 /*none*/,
        /*C4*/ 0 /*NONE*/,
        /*C5*/ 0 /*NONE*/,
        /*C6*/ 0 /*NONE*/,
        /*C7*/ 0 /*NONE*/,
        /*C8*/ 0 /*NONE*/,
        /*C9*/ 0 /*NONE*/,
        /*CA*/ 0 /*NONE*/,
        /*CB*/ 0 /*NONE*/,
        /*CC*/ 0 /*NONE*/,
        /*CD*/ 0 /*NONE*/,
        /*CE*/ 0 /*NONE*/,
        /*CF*/ 0 /*NONE*/,
        /*D0*/ 0 /*none*/,
        /*D1*/ 0 /*none*/,
        /*D2*/ 0 /*none*/,
        /*D3*/ 0 /*none*/,
        /*D4*/ 0 /*NONE*/,
        /*D5*/ 0 /*NONE*/,
        /*D6*/ 0 /*NONE*/,
        /*D7*/ 0 /*NONE*/,
        /*D8*/ 0 /*NONE*/,
        /*D9*/ 0 /*NONE*/,
        /*DA*/ 0 /*NONE*/,
        /*DB*/ 0 /*NONE*/,
        /*DC*/ 0 /*NONE*/,
        /*DD*/ 0 /*NONE*/,
        /*DE*/ 0 /*NONE*/,
        /*DF*/ 0 /*NONE*/,
        /*E0*/ 0x14 /*LCtrl*/,
        /*E1*/ 0x12 /*LShift*/,
        /*E2*/ 0x11 /*LAlt*/,
        /*E3*/ 0 /*LGUI*/,
        /*E4*/ 0x114 /*RCtrl*/,
        /*E5*/ 0x59 /*RShift*/,
        /*E6*/ 0x111 /*RAlt*/,
        /*E7*/ 0 /*RGUI*/
};

#define NUM_USBCODES (sizeof(usbcodes) / sizeof(usbcodes[0]))

constexpr uint32_t _keycode_type2_to_usb(uint32_t keycode) {
    uint32_t unmasked = keycode & ~(KEYBOARD_CODE_MASK);
    uint32_t masked_code = keycode & KEYBOARD_CODE_MASK;
    for (uint32_t code = 0; code < NUM_USBCODES; code++) {
        if (masked_code == usbcodes[code]) {
            return unmasked | code;
        }
    }
    return unmasked;
}

uint32_t keycode_type2_to_usb(uint32_t keycode) {
    return _keycode_type2_to_usb(keycode);
}
