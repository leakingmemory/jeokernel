//
// Created by sigsegv on 6/5/21.
//

#include <cpuio.h>
#include <sstream>
#include <klogger.h>
#include <chrono>
#include <thread>
#include "ps2.h"
#include "ps2kbd.h"

void ps2::init() {
    command(PS2_DISABLE_PORT1);
    command(PS2_DISABLE_PORT2);
    while (status_output_full(status())) {
        input();
    }
    command(PS2_READ_BYTE00);
    bool dual_channel{true};
    uint8_t config = input();
    if ((config & PS2_CONFIG_PORT2_CLOCK_OFF) == 0) {
        dual_channel = false;
    }
    config &= ~(PS2_CONFIG_PORT1_IRQ | PS2_CONFIG_PORT2_IRQ | PS2_CONFIG_PORT1_TRANSLATION);
    command(PS2_WRITE_BYTE00, config);
    command(PS2_SELFTEST);
    {
        uint8_t result{0};
        do {
            result = input();
        } while (result != 0x55 && result != 0xFC);
        if (result == 0xFC) {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Failed self test\n";
            get_klogger() << msg.str().c_str();
            return;
        }
    }
    command(PS2_WRITE_BYTE00, config);
    if (dual_channel) {
        using namespace std::literals::chrono_literals;
        command(PS2_ENABLE_PORT2);
        std::this_thread::sleep_for(20ms);
        if ((config & PS2_CONFIG_PORT2_CLOCK_OFF) != 0) {
            dual_channel = false;
        }
        command(PS2_DISABLE_PORT2);
    }
    command(PS2_TEST_PORT1);
    port1 = true;
    port2 = dual_channel;
    {
        uint8_t result{0};
        do {
            result = input();
        } while (result > 4);
        if (result != 0) {
            port1 = false;
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Port 1 does not work " << (unsigned int) result <<  "\n";
            get_klogger() << msg.str().c_str();
        }
    }
    if (port2) {
        command(PS2_TEST_PORT2);
        uint8_t result{0};
        do {
            result = input();
        } while (result > 4);
        if (result != 0) {
            port2 = false;
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Port 2 does not work " << (unsigned int) result <<  "\n";
            get_klogger() << msg.str().c_str();
        }
    }
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": PS/2 controller, " << (dual_channel ? "dual channel" : "single channel") <<  "\n";
        get_klogger() << msg.str().c_str();
    }
    if (port1) {
        command(PS2_ENABLE_PORT1);
        config |= PS2_CONFIG_PORT1_IRQ;
    }
    if (port2) {
        command(PS2_ENABLE_PORT2);
        config |= PS2_CONFIG_PORT2_IRQ;
    }
    if (port1) {
        drain_input();
        bool successful_initial_reset{iface1.reset()};
        drain_input();
        if (successful_initial_reset) {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Port 1 reset successful\n";
            get_klogger() << msg.str().c_str();
        } else {
            port1 = false;
        }
    }
    if (port2) {
        drain_input();
        bool successful_initial_reset{iface2.reset()};
        drain_input();
        if (successful_initial_reset) {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Port 2 reset successful\n";
            get_klogger() << msg.str().c_str();
        }
    } else {
        port2 = false;
    }
    ProbeDevices();
}

void ps2::command(uint8_t cmd) {
    wait_ready_for_input();
    outportb(PS2_COMMAND_STATUS, cmd);
}

uint8_t ps2::status() {
    return inportb(PS2_COMMAND_STATUS);
}

uint8_t ps2::input() {
    return inportb(PS2_DATA);
}

void ps2::output(uint8_t data) {
    wait_ready_for_input();
    outportb(PS2_DATA, data);
}

void ps2::command(uint8_t cmd, uint8_t data) {
    command(cmd);
    output(data);
}

void ps2::wait_ready_for_input() {
    for (int i = 0; i < 10; i++) {
        if ((status() & PS2_STATUS_INPUTBUFFER) == 0) {
            return;
        }
    }
    while ((status() & PS2_STATUS_INPUTBUFFER) != 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
    }
}

void ps2::drain_input() {
    using namespace std::literals::chrono_literals;
    if (status_output_full(status())) {
        input();
        std::this_thread::sleep_for(10ms);
    } else {
        std::this_thread::sleep_for(10ms);
    }
    while (status_output_full(status())) {
        input();
        std::this_thread::sleep_for(10ms);
    }
}

bool ps2::spawn(ps2_device_interface &dev) {
    if (dev.device_id == 0x83AB) {
        ps2kbd *kbd = new ps2kbd(*this, dev);
        devices().add(*kbd);
        kbd->init();
        return true;
    }
    return false;
}

void ps2::ProbeDevices() {
    if (port1) {
        std::optional<uint16_t> ident = iface1.identify();
        if (ident) {
            iface1.device_id = *ident;
            if (!spawn(iface1)) {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": Port 1 identified " << std::hex
                    << (unsigned int) *ident << "\n";
                get_klogger() << msg.str().c_str();
            }
        } else {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Port 1 not responding\n";
            get_klogger() << msg.str().c_str();
        }
    }
    if (port2) {
        std::optional<uint16_t> ident = iface2.identify();
        if (ident) {
            iface2.device_id = *ident;
            if (!spawn(iface2)) {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": Port 2 identified " << std::hex
                    << (unsigned int) *ident << "\n";
                get_klogger() << msg.str().c_str();
            }
        } else {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": Port 2 not responding\n";
            get_klogger() << msg.str().c_str();
        }
    }
    Bus::ProbeDevices();
}

void ps2_device_interface::send(uint8_t data) {
    if (port2) {
        ps2bus->command(PS2_SEND_TO_PORT2);
    }
    ps2bus->output(data);
}

bool ps2_device_interface::reset() {
    send(PS2_DEVICE_RESET);
    uint8_t timeout = 0xFF;
    while (!ps2bus->status_output_full(ps2bus->status())) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
        if (--timeout == 0) {
            break;
        }
    }
    if (timeout > 0) {
        uint8_t result = ps2bus->input();
        if (result == PS2_DEVICE_SUCCESS) {
            return true;
        } else {
            std::stringstream msg;
            msg << ps2bus->DeviceType() << (unsigned int) ps2bus->DeviceId() << ": Port " << (port2 ? "2" : "1") << " error " << std::hex << (unsigned int) result << "\n";
            get_klogger() << msg.str().c_str();
        }
    } else {
        std::stringstream msg;
        msg << ps2bus->DeviceType() << (unsigned int) ps2bus->DeviceId() << ": Port " << (port2 ? "2" : "1") << " timeout\n";
        get_klogger() << msg.str().c_str();
    }
    return false;
}

std::optional<uint16_t> ps2_device_interface::identify() {
    const uint8_t bits = 16;
    uint8_t shift = 0;
    uint16_t value{0};
    send(PS2_DEVICE_IDENT);
    {
        uint8_t timeout = 0xFF;
        while (--timeout > 0) {
            if (ps2bus->status_output_full(ps2bus->status())) {
                uint8_t result = ps2bus->input();
                if (result != PS2_DEVICE_SUCCESS) {
                    std::stringstream msg;
                    msg << ps2bus->DeviceType() << (unsigned int) ps2bus->DeviceId() << ": Ident error code " << std::hex << (unsigned int) result << "\n";
                    get_klogger() << msg.str().c_str();
                    return { };
                }
                break;
            }
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
        if (timeout == 0) {
            std::stringstream msg;
            msg << ps2bus->DeviceType() << (unsigned int) ps2bus->DeviceId() << ": Ident timeout\n";
            get_klogger() << msg.str().c_str();
            return { };
        }
    }
    bool received{true};
    while (received) {
        received = false;
        uint8_t timeout = 0xFF;
        while (--timeout > 0) {
            if (ps2bus->status_output_full(ps2bus->status())) {
                uint16_t incoming = ps2bus->input();
                if (shift < bits) {
                    incoming = incoming << shift;
                    value |= incoming;
                    shift += 8;
                } else {
                    std::stringstream msg;
                    msg << ps2bus->DeviceType() << (unsigned int) ps2bus->DeviceId() << ": Ident overrun bits " << std::hex << (unsigned int) incoming << "\n";
                    get_klogger() << msg.str().c_str();
                }
                received = true;
                break;
            }
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }
    if (shift > 0) {
        return { value };
    } else {
        return { };
    }
}

void ps2_device_interface::EnableIrq(bool enable) {
    std::lock_guard lock(ps2bus->mtx);
    ps2bus->command(PS2_READ_BYTE00);
    uint8_t config = ps2bus->input();
    uint8_t flag{PS2_CONFIG_PORT1_IRQ};
    if (port2) {
        flag = PS2_CONFIG_PORT2_IRQ;
    }
    if (enable) {
        config |= flag;
    } else {
        config &= ~flag;
    }
    ps2bus->command(PS2_WRITE_BYTE00, config);
    ps2bus->drain_input();
}
