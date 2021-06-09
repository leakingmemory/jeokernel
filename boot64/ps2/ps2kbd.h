//
// Created by sigsegv on 6/6/21.
//

#ifndef JEOKERNEL_PS2KBD_H
#define JEOKERNEL_PS2KBD_H

#include "ps2.h"
#include "../IOApic.h"
#include <core/LocalApic.h>
#include <memory>
#include <concurrency/hw_spinlock.h>
#include <concurrency/raw_semaphore.h>
#include <keyboard/keyboard.h>

#define RD_RING_BUFSIZE_BITS 4
#define RD_RING_BUFSIZE      (1 << RD_RING_BUFSIZE_BITS)
#define RD_RING_BUFSIZE_MASK (RD_RING_BUFSIZE - 1)

class ps2kbd;

class ps2kbd_type2_state_machine : public keyboard_type2_state_machine {
private:
    ps2kbd &kbd;
public:
    explicit ps2kbd_type2_state_machine(ps2kbd &kbd);

    void SetLeds(bool capslock, bool scrolllock, bool numlock) override;

    void keycode(uint32_t code) override;
};

class ps2kbd : public Device {
    friend ps2kbd_type2_state_machine;
private:
    ps2kbd_type2_state_machine code_state_machine;
    hw_spinlock spinlock;
    raw_semaphore sema;
    raw_semaphore command_sema;
    bool stop;
    std::thread extractor_thread;
    ps2_device_interface &ps2dev;
    std::unique_ptr<IOApic> ioapic;
    std::unique_ptr<LocalApic> lapic;
    uint8_t buffer[RD_RING_BUFSIZE];
    uint16_t inspos, outpos;
    uint8_t cmd_length;
    uint8_t cmd[2];
public:
    ps2kbd(ps2 &ps2, ps2_device_interface &ps2dev) : Device("ps2kbd", &ps2),
    code_state_machine(*this),
    spinlock(), sema(-1), command_sema(0), stop(false),
    extractor_thread([this] () { this->extractor(); }), ps2dev(ps2dev), ioapic(), lapic(),
    buffer(), inspos(0), outpos(0), cmd_length(0), cmd() {}
    ~ps2kbd() {
        {
            critical_section cli{};
            std::lock_guard lock(spinlock);
            stop = true;
            sema.release();
        }
        extractor_thread.join();
    }
    void init() override;
    void extractor();
    void SetLeds(bool capslock, bool scrolllock, bool numlock);
    void keycode(uint32_t code);
};


#endif //JEOKERNEL_PS2KBD_H
