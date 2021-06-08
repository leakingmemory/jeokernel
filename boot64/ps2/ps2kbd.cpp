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
#include <keyboard/keyboard.h>

//#define DEBUG_BUFFER

void ps2kbd::init() {
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Init\n";
        get_klogger() << msg.str().c_str();
    }
    ps2dev.EnableIrq();
    cpu_mpfp *mpfp = get_mpfp();
    uint8_t ioapic_intn{ps2dev.IrqNum()};
    {
        uint8_t isa_bus_id{0xFF};
        for (int i = 0; i < mpfp->get_num_bus(); i++) {
            const mp_bus_entry &bus = mpfp->get_bus(i);
            if (bus.bus_type[0] == 'I' && bus.bus_type[1] == 'S' && bus.bus_type[2] == 'A') {
                isa_bus_id = bus.bus_id;
            }
        }
        {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": ISA Bus ID " << (unsigned int) isa_bus_id << "\n";
            get_klogger() << msg.str().c_str();
        }
        for (int i = 0; i < mpfp->get_num_ioapic_ints(); i++) {
            const mp_ioapic_interrupt_entry &ioapic_int = mpfp->get_ioapic_int(i);
            if (ioapic_int.source_bus_irq == ps2dev.IrqNum() && ioapic_int.source_bus_id == isa_bus_id) {
                ioapic_intn = ioapic_int.ioapic_intin;

                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": IRQ bus "
                    << (unsigned int) ioapic_int.source_bus_id << " "
                    << (unsigned int) ioapic_int.source_bus_irq << " -> "
                    << (unsigned int) ioapic_int.ioapic_intin << " flags "
                    << (unsigned int) ioapic_int.interrupt_flags << "\n";
                get_klogger() << msg.str().c_str();
            }
        }
    }

    lapic = std::make_unique<LocalApic>(*mpfp);
    ioapic = std::make_unique<IOApic>(*mpfp);

    get_hw_interrupt_handler().add_handler(4, [this, ioapic_intn] (Interrupt &intr) {
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

    ioapic->enable(ioapic_intn, lapic->get_cpu_num(*mpfp));
    ioapic->send_eoi(ioapic_intn);

    SetLeds(true, true, true);
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(500ms);
    }
    SetLeds(false, false, true);
}

void ps2kbd::extractor() {
    while (true) {
        sema.acquire();
        uint8_t ch{0};

#ifdef DEBUG_BUFFER
        std::stringstream msg{};
#endif
        {
            critical_section cli{};
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
                this->ps2dev.Bus().wait_ready_for_input();
                this->ps2dev.send(cmd[0]);
            } else {
                command_sema.release();
            }
        } else {
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
                            this->keycode(keycode);
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
                            this->keycode(keycode);
                        }
                }
            }
        }
    }
}

void ps2kbd::SetLeds(bool capslock, bool scrolllock, bool numlock) {
    std::lock_guard lock(ps2dev.Mtx());
    this->capslock = capslock;
    this->scrolllock = scrolllock;
    this->numlock = numlock;
    uint8_t leds = (capslock ? 4 : 0) | (numlock ? 2 : 0) | (numlock ? 1 : 0);
    this->cmd[0] = 0xED;
    this->cmd[1] = leds;
    this->cmd_length = 2;
    this->command_sema.acquire();
    this->ps2dev.Bus().wait_ready_for_input();
    this->ps2dev.send(cmd[0]);
}

void ps2kbd::keycode(uint16_t code) {
    get_klogger() << code;
}
