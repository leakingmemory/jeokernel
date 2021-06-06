//
// Created by sigsegv on 6/5/21.
//

#ifndef JEOKERNEL_PS2_H
#define JEOKERNEL_PS2_H

#include <devices/drivers.h>
#include <optional>

#define PS2_DATA 0x60
#define PS2_COMMAND_STATUS 0x64

#define PS2_READ_BYTE00   0x20
#define PS2_WRITE_BYTE00  0x60
#define PS2_DISABLE_PORT2 0xA7
#define PS2_ENABLE_PORT2  0xA8
#define PS2_TEST_PORT2    0xA9
#define PS2_SELFTEST      0xAA
#define PS2_TEST_PORT1    0xAB
#define PS2_DISABLE_PORT1 0xAD
#define PS2_ENABLE_PORT1  0xAE
#define PS2_SEND_TO_PORT2 0xD4

#define PS2_STATUS_OUTPUTBUFFER 0x01
#define PS2_STATUS_INPUTBUFFER  0x02

#define PS2_CONFIG_PORT1_IRQ         0x01
#define PS2_CONFIG_PORT2_IRQ         0x02
#define PS2_CONFIG_PORT1_CLOCK_OFF   0x10
#define PS2_CONFIG_PORT2_CLOCK_OFF   0x20
#define PS2_CONFIG_PORT1_TRANSLATION 0x40

#define PS2_DEVICE_IDENT    0xF2
#define PS2_DEVICE_RESET    0xFF

#define PS2_DEVICE_SUCCESS 0xFA

class ps2;

class ps2_device_interface {
    friend ps2;
private:
    ps2 *ps2bus;
    uint16_t device_id;
    bool port2 : 8;
public:
    ps2_device_interface(ps2 *ps2bus, bool port2) : ps2bus(ps2bus), port2(port2) {}
    void send(uint8_t data);
    bool reset();
    std::optional<uint16_t> identify();

    constexpr uint8_t IrqNum() const {
        return port2 ? 12 : 1;
    }
};

class ps2 : public Bus {
private:
    bool port1, port2;
    ps2_device_interface iface1, iface2;
public:
    ps2() : Bus("ps"), port1(false), port2(false), iface1(this, false), iface2(this, true) {}
    void init() override;
    void ProbeDevices() override;
    void command(uint8_t cmd);
    void command(uint8_t cmd, uint8_t data);
    uint8_t status();
    uint8_t input();
    void output(uint8_t data);
    void wait_ready_for_input();
    void drain_input();
    bool spawn(ps2_device_interface &dev);
    bool IsBus() override {
        return true;
    }

    constexpr bool status_output_full(uint8_t status) const {
        return (status & PS2_STATUS_OUTPUTBUFFER) != 0;
    }
};

#endif //JEOKERNEL_PS2_H
