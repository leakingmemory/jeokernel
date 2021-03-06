//
// Created by sigsegv on 6/6/21.
//

#include <sstream>
#include <klogger.h>
#include <core/cpu_mpfp.h>
#include <core/LocalApic.h>
#include <thread>
#include "ps2kbd.h"
#include "../HardwareInterrupts.h"
#include "../ApStartup.h"
#include <keyboard/keyboard.h>

//#define DEBUG_BUFFER

void ps2kbd::init() {
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Init\n";
        get_klogger() << msg.str().c_str();
    }
    ps2dev.EnableIrq();
    uint8_t ioapic_intn{ps2dev.IrqNum()};

    ApStartup *ap = GetApStartup();

    ioapic_intn = ap->GetIsaIoapicPin(ioapic_intn);
    {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": IRQ " << ioapic_intn << "\n";
        get_klogger() << str.str().c_str();
    }

    lapic = ap->GetLocalApic();
    ioapic = ap->GetIoapic();

    /* int pin number + 3 local apic ints below ioapic range */
    get_hw_interrupt_handler().add_handler(ioapic_intn + 3, [this, ioapic_intn] (Interrupt &intr) {
        bool dispatch{false};
        {
            std::lock_guard lock{spinlock};
            uint8_t ch = ps2dev.Bus().input();
            uint32_t newpos = (inspos + 1) & RD_RING_BUFSIZE_MASK;
#ifdef DEBUG_BUFFER
            std::stringstream msg{};
#endif
            if (newpos != outpos) {
#ifdef DEBUG_BUFFER
                msg << " (" << std::hex << (unsigned int) inspos << "/" << outpos << ")<=" << (unsigned int) ch;
#endif
                buffer[inspos] = ch;
                inspos = newpos;
                dispatch = true;
            }
#ifdef DEBUG_BUFFER
            get_klogger() << msg.str().c_str();
#endif
        }
        if (dispatch) {
            sema.release();
        }
        ioapic->send_eoi(ioapic_intn);
        lapic->eio();
    });

    ioapic->enable(ioapic_intn, lapic->get_lapic_id() >> 24);
    ioapic->send_eoi(ioapic_intn);

    code_state_machine.SetLeds(true, true, true);
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(500ms);
    }
    code_state_machine.SetLeds(false, false, true);
}

void ps2kbd::extractor() {
    while (true) {
        sema.acquire();
        uint8_t ch{0};

#ifdef DEBUG_BUFFER
        std::stringstream msg{};
#endif
        {
            std::lock_guard lock{spinlock};
            if (stop) {
                return;
            }
#ifdef DEBUG_BUFFER
            msg << " (" << std::hex << (unsigned int) outpos << ")=>";
#endif
            ch = buffer[outpos];
#ifdef DEBUG_BUFFER
            msg << (unsigned int) ch;
#endif
            outpos = (outpos + 1) & RD_RING_BUFSIZE_MASK;
        }
#ifdef DEBUG_BUFFER
        get_klogger() << msg.str().c_str();
#endif
        if (this->cmd_length > 0 && (ch == 0xFA || ch == 0xFE)) {
            if (ch == 0xFA) {
                this->cmd_length--;
                for (int i = 0; i < this->cmd_length; i++) {
                    this->cmd[i] = this->cmd[i + 1];
                }
            }
            if (this->cmd_length > 0) {
                std::lock_guard lock(ps2dev.Mtx());
                this->ps2dev.Bus().wait_ready_for_input();
                this->ps2dev.send(cmd[0]);
            } else {
                command_sema.release();
            }
        } else {
            code_state_machine.raw_code(ch);
        }
    }
}

void ps2kbd::SetLeds(bool capslock, bool scrolllock, bool numlock) {
    uint8_t leds = (capslock ? 4 : 0) | (numlock ? 2 : 0) | (numlock ? 1 : 0);
    this->cmd[0] = 0xED;
    this->cmd[1] = leds;
    this->cmd_length = 2;
    this->command_sema.acquire();
    std::lock_guard lock(ps2dev.Mtx());
    this->ps2dev.Bus().wait_ready_for_input();
    this->ps2dev.send(cmd[0]);
}

void ps2kbd::keycode(uint32_t code) {
    Keyboard().keycode(keycode_type2_to_usb(code));
}

ps2kbd_type2_state_machine::ps2kbd_type2_state_machine(ps2kbd &kbd) : kbd(kbd) {
}

void ps2kbd_type2_state_machine::SetLeds(bool capslock, bool scrolllock, bool numlock) {
    keyboard_type2_state_machine::SetLeds(capslock, scrolllock, numlock);
    kbd.SetLeds(capslock, scrolllock, numlock);
}

void ps2kbd_type2_state_machine::keycode(uint32_t code) {
    kbd.keycode(code);
}
