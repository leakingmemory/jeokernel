//
// Created by sigsegv on 04.05.2021.
//

#ifndef JEOKERNEL_PITTIMERCALIB_H
#define JEOKERNEL_PITTIMERCALIB_H

#include <cstdint>

class PITTimerCalib {
private:
    uint8_t hw_mode;

    void command(uint8_t cmd);
    void ch2_data(uint8_t data);
    void iodelay();
    void ch2_data(uint16_t data);
    void ch2_reset(uint16_t microseconds);
    bool volatile ch2_pit_high();
public:
    PITTimerCalib();
    ~PITTimerCalib();
    void delay(uint16_t microseconds);

    void leave_pit_off() {
        hw_mode = hw_mode & 0xFE;
    }
    void leave_pit_on() {
        hw_mode = hw_mode | 0x01;
    }
    void leave_speaker_off() {
        hw_mode = hw_mode &0xFD;
    }
    void leave_speaker_on() {
        hw_mode = hw_mode | 0x02;
    }

    PITTimerCalib(PITTimerCalib &&) = delete;
    PITTimerCalib(const PITTimerCalib &) = delete;
    PITTimerCalib & operator = (PITTimerCalib &&) = delete;
    PITTimerCalib & operator = (const PITTimerCalib &) = delete;
};


#endif //JEOKERNEL_PITTIMERCALIB_H
