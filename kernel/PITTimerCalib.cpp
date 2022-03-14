//
// Created by sigsegv on 04.05.2021.
//

#include "PITTimerCalib.h"
#include <cpuio.h>

PITTimerCalib::PITTimerCalib() {
    hw_mode = inportb(0x61);
    outportb(0x61, (hw_mode & 0xFD /* speaker=off */) | 1 /* timer=on */);
}

PITTimerCalib::~PITTimerCalib() {
    outportb(0x61, hw_mode);
}

void PITTimerCalib::command(uint8_t cmd) {
    outportb(0x43, cmd);
}

void PITTimerCalib::ch2_data(uint8_t data) {
    outportb(0x42, data);
}

void PITTimerCalib::iodelay() {
    inportb(0x60);
}

void PITTimerCalib::ch2_data(uint16_t data) {
    ch2_data((uint8_t) (data & 0xFF));
    ch2_data((uint8_t) (data >> 8));
}

void PITTimerCalib::ch2_reset(uint16_t microseconds) {
    uint16_t value = microseconds;
    {
        uint64_t calc = microseconds;
        calc *= 1193181667;
        calc /= 1000000000;
        value = calc;
    }

    uint8_t hwm = inportb(0x61);
    outportb(0x61, hwm & 0xFE); // off
    command(0xB0);
    ch2_data(value);
    hwm = inportb(0x61);
    outportb(0x61, hwm | 0x01); // on
}

void PITTimerCalib::delay(uint16_t microseconds) {
    ch2_reset(microseconds);
    while (!ch2_pit_high()) {
        asm("nop");
    }
}

bool PITTimerCalib::ch2_pit_high() {
    return (inportb(0x61) & 0x20) != 0;
}
