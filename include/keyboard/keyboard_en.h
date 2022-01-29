//
// Created by sigsegv on 1/29/22.
//

#ifndef JEOKERNEL_KEYBOARD_EN_H
#define JEOKERNEL_KEYBOARD_EN_H

#include <keyboard/keyboard.h>

class keyboard_en_codepage : public keyboard_codepage {
public:
    uint32_t Translate(uint32_t keycode) override;
};

#endif //JEOKERNEL_KEYBOARD_EN_H
