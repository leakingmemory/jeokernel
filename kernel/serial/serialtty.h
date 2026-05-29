//
// Created by sigsegv on 5/17/26.
//

#ifndef JEOKERNEL_SERIALTTY_H
#define JEOKERNEL_SERIALTTY_H

#include <devices/devices.h>
#include <concurrency/hw_spinlock.h>

class serialport;
class IOApic;
class LocalApic;

class serialtty : public Device, public KLogger {
private:
    hw_spinlock spinlock;
    serialport *sport;
    IOApic *ioapic;
    LocalApic *lapic;
    char *outbuf;
    size_t outbuf_size;
    size_t outbuf_off;
    size_t outbuf_len;
    uint8_t int_enable{0};
public:
    serialtty(serialport *sport);
    void init() override;
private:
    int increase_size(size_t minimum);
    size_t append_buf(const char *output, size_t len);
public:
    int write(const char *output, size_t len);
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    void erase(int backtrack, int erase) override;
    KLogger & operator << (const char *str) override;

    bool has_input() const override;
};


#endif //JEOKERNEL_SERIALTTY_H
