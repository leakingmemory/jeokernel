//
// Created by sigsegv on 5/12/26.
//

#include "serialport.h"
#include <cpuio.h>

#include "klogger.h"

void serialport::set_interrupt_enable(uint8_t enbl) const {
    outportb(ioport + 1, enbl);
}

void serialport::disable_interrupts() const {
    set_interrupt_enable(0);
}

void serialport::set_linecontrol(uint8_t ctrl) const {
    outportb(ioport + 3, ctrl);
}

void serialport::set_to_dlab() const {
    set_linecontrol(0x80);
}

void serialport::set_divisor_in_dlab_mode(uint16_t divisor) const {
    outportb(ioport + 0, static_cast<uint8_t>(divisor & 0xff));
    outportb(ioport + 1, static_cast<uint8_t>((divisor >> 8) & 0xff));
}

void serialport::set_fifo_control(uint8_t ctrl) const {
    outportb(ioport + 2, ctrl);
}

void serialport::set_modem_control(uint8_t ctrl) const {
    outportb(ioport + 4, ctrl);
}

void serialport::output(uint8_t data) const {
    outportb(ioport, data);
}

uint8_t serialport::input() const {
    return inportb(ioport);
}

bool serialport::probe() const {
    disable_interrupts();
    set_to_dlab();
    set_divisor_in_dlab_mode(3); // 38400 baud
    set_linecontrol(0x03); // 8 bits, no parity, one stop bit
    set_fifo_control(0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    set_modem_control(0x0B);    // IRQs enabled, RTS/DSR set
    set_modem_control(0x1E);    // Set in loopback mode, test the serial chip
    output(0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)
    if (input() == 0xAE) {
        set_modem_control(0x0F); // Output mode
        get_klogger() << "Serial port " << ioport << " available\n";
        return true;
    } else {
        get_klogger() << "Serial port " << ioport << " not available\n";
        return false;
    }
}

bool serialport::has_data() const {
    return (inportb(ioport + 5) & 1) != 0;
}
uint8_t serialport::read() const {
    while (!has_data()) {
    }
    return input();
}
bool serialport::can_write() const {
    return (inportb(ioport + 5) & 0x20) != 0;
}
void serialport::write(uint8_t data) const {
    while (!can_write()) {
    }
    output(data);
}

uint32_t serialport::GetWidth() {
    return 0;
}
uint32_t serialport::GetHeight() {
    return 0;
}
void serialport::print_at(uint8_t col, uint8_t row, const char *str) {
}
void serialport::erase(int backtrack, int erase) {
}
KLogger & serialport::operator << (const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            write('\r');
        }
        write(*str);
        ++str;
    }
    return *this;
}

bool serialport::has_input() const {
    return true;
}
