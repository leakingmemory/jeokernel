//
// Created by sigsegv on 9/17/26.
//

#ifndef JEOKERNEL_UART_H
#define JEOKERNEL_UART_H

#include <cstdint>
#include "klogger.h"

class uart : public KLogger {
private:
    volatile uint32_t *base;
    uint8_t irq;
public:
    constexpr uart(volatile void *base, uint8_t irq = 0)
        : base(reinterpret_cast<volatile uint32_t *>(base)), irq(irq) {
    }
    constexpr uart(uintptr_t base_addr, uint8_t irq = 0)
        : base(reinterpret_cast<volatile uint32_t *>(base_addr)), irq(irq) {
    }

    [[nodiscard]] constexpr uint8_t get_irq() const {
        return irq;
    }

public:
    void set_interrupt_enable(uint8_t enbl) const;
    void set_up() const;

private:
    void disable_interrupts() const;
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

#endif //JEOKERNEL_UART_H
