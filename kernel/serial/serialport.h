//
// Created by sigsegv on 5/12/26.
//

#ifndef JEOKERNEL_SERIALPORT_H
#define JEOKERNEL_SERIALPORT_H

#include <cstdint>

#include "klogger.h"

class serialport : public KLogger {
    uint16_t ioport;
    uint8_t irq;
public:
    constexpr serialport(uint16_t ioport, uint8_t irq) : ioport(ioport), irq(irq) {
    }
    [[nodiscard]] constexpr uint8_t get_irq() const {
        return irq;
    }
public:
    void set_interrupt_enable(uint8_t enbl) const;
private:
    void disable_interrupts() const;
    void set_linecontrol(uint8_t ctrl) const;
    void set_to_dlab() const;
    void set_divisor_in_dlab_mode(uint16_t divisor) const;
    void set_fifo_control(uint8_t ctrl) const;
    void set_modem_control(uint8_t ctrl) const;
    void output(uint8_t data) const;
    [[nodiscard]] uint8_t input() const;
public:
    bool probe() const;
    bool has_data() const;
    uint8_t read() const;
    bool can_write() const;
    void write(uint8_t data) const;

    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    void erase(int backtrack, int erase) override;
    KLogger & operator << (const char *str) override;

    bool has_input() const override;
};


#endif //JEOKERNEL_SERIALPORT_H
