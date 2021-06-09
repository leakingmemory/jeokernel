//
// Created by sigsegv on 6/8/21.
//

#ifndef JEOKERNEL_KEYBOARD_H
#define JEOKERNEL_KEYBOARD_H

#define KEYBOARD_CODE_BIT_RELEASE    0x8000
#define KEYBOARD_CODE_BIT_SCROLLLOCK 0x4000
#define KEYBOARD_CODE_BIT_CAPSLOCK   0x2000
#define KEYBOARD_CODE_BIT_NUMLOCK    0x1000
#define KEYBOARD_CODE_BIT_LCONTROL   0x0800
#define KEYBOARD_CODE_BIT_RCONTROL   0x0400

#define KEYBOARD_CODE_MASK           0x03FF

#define KEYBOARD_CODE_EXTENDED    0x0100

#define KEYBOARD_CODE_PRINTSCREEN 0x0200
#define KEYBOARD_CODE_PAUSE       (0x0201 | KEYBOARD_CODE_BIT_RELEASE)

#include <cstdint>

class keyboard_type2_state_machine {
private:
    uint16_t state;
    bool capslock : 2;
    bool scrolllock : 2;
    bool numlock : 4;
    uint8_t ignore_length;
    uint8_t recorded_length;
    uint8_t ignore_codes[2];
#define REC_BUFFER_SIZE 7
    uint8_t recorded_codes[REC_BUFFER_SIZE];
public:
    keyboard_type2_state_machine() :
    state(0), capslock(false), scrolllock(false), numlock(false), ignore_length(0), recorded_length(0),
    ignore_codes(), recorded_codes()
    {}
    virtual void SetLeds(bool capslock, bool scrolllock, bool numlock);
    virtual void keycode(uint16_t code) {}
    void layer2_keycode(uint16_t code);
    void raw_code(uint8_t ch);
};

#endif //JEOKERNEL_KEYBOARD_H
