//
// Created by sigsegv on 6/8/21.
//

#ifndef JEOKERNEL_KEYBOARD_H
#define JEOKERNEL_KEYBOARD_H

#define KEYBOARD_CODE_BIT_RELEASE    0x0080000
#define KEYBOARD_CODE_BIT_SCROLLLOCK 0x0040000
#define KEYBOARD_CODE_BIT_CAPSLOCK   0x0020000
#define KEYBOARD_CODE_BIT_NUMLOCK    0x0010000

#define KEYBOARD_CODE_BIT_LSHIFT     0x0100000
#define KEYBOARD_CODE_BIT_RSHIFT     0x0200000
#define KEYBOARD_CODE_BIT_LALT       0x0400000
#define KEYBOARD_CODE_BIT_RALT       0x0800000
#define KEYBOARD_CODE_BIT_LCONTROL   0x1000000
#define KEYBOARD_CODE_BIT_RCONTROL   0x2000000
#define KEYBOARD_CODE_BIT_LGUI       0x4000000
#define KEYBOARD_CODE_BIT_RGUI       0x8000000

#define KEYBOARD_CODE_MASK           0xFFFF

#define KEYBOARD_CODE_EXTENDED    0x0100

#define KEYBOARD_CODE_PRINTSCREEN 0x0200
#define KEYBOARD_CODE_PAUSE       (0x0201 | KEYBOARD_CODE_BIT_RELEASE)

#define KEYBOARD_CODE_BACKSPACE   0x2A

#include <cstdint>
#include "concurrency/hw_spinlock.h"
#include "vector"
#include "string"
#include "concurrency/raw_semaphore.h"

class keyboard_type2_state_machine {
private:
    uint32_t state;
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
    virtual void keycode(uint32_t code) {}
    void layer2_keycode(uint32_t code);
    void raw_code(uint8_t ch);
};

uint32_t keycode_type2_to_usb(uint32_t keycode);

class keyboard_codepage {
public:
    virtual uint32_t Translate(uint32_t keycode) = 0;
};

class keycode_consumer {
public:
    virtual ~keycode_consumer() = default;
    virtual bool Consume(uint32_t keycode) = 0;
};

class keyboard {
private:
    hw_spinlock lock;
    std::shared_ptr<keycode_consumer> consumer;
    std::vector<std::shared_ptr<keycode_consumer>> consumers;
public:
    keyboard() : lock(), consumer(), consumers() {}
    void keycode(uint32_t code);
    void consume(std::shared_ptr<keycode_consumer> consumer);
    void unconsume(std::shared_ptr<keycode_consumer> consumer);
};

keyboard &Keyboard();
void KeyboardCodepage(std::shared_ptr<keyboard_codepage> codepage);
std::shared_ptr<keyboard_codepage> KeyboardCodepage();

class keyboard_blocking_string {
public:
    virtual const std::string &GetString() = 0;
    virtual keyboard_blocking_string *GetBlockingString() {
        return this;
    }
};

class keyboard_line_consumer : public keycode_consumer, private keyboard_blocking_string {
private:
    raw_semaphore sema;
    std::shared_ptr<keyboard_codepage> codepage;
    std::string str;
public:
    explicit keyboard_line_consumer(std::shared_ptr<keyboard_codepage> codepage) : keyboard_blocking_string(), sema(-1), codepage(codepage), str() {}
    keyboard_line_consumer() : keyboard_line_consumer(KeyboardCodepage()) {}
    bool Consume(uint32_t keycode) override;
    keyboard_blocking_string *GetBlockingString() override {
        return keyboard_blocking_string::GetBlockingString();
    }
private:
    const std::string &GetString() override;
};

#endif //JEOKERNEL_KEYBOARD_H
