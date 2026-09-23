//
// Created by sigsegv on 9/17/26.
//

#include "uart.h"
#include "klogger.h"

namespace {
    // PL011 Register Offsets (in 32-bit words)
    constexpr size_t REG_UARTDR    = 0x00 / sizeof(uint32_t);
    constexpr size_t REG_UARTRSR   = 0x04 / sizeof(uint32_t);
    constexpr size_t REG_UARTFR    = 0x18 / sizeof(uint32_t);
    constexpr size_t REG_UARTIBRD  = 0x24 / sizeof(uint32_t);
    constexpr size_t REG_UARTFBRD  = 0x28 / sizeof(uint32_t);
    constexpr size_t REG_UARTLCR_H = 0x2C / sizeof(uint32_t);
    constexpr size_t REG_UARTCR    = 0x30 / sizeof(uint32_t);
    constexpr size_t REG_UARTIFLS  = 0x34 / sizeof(uint32_t);
    constexpr size_t REG_UARTIMSC  = 0x38 / sizeof(uint32_t);
    constexpr size_t REG_UARTRIS   = 0x3C / sizeof(uint32_t);
    constexpr size_t REG_UARTMIS   = 0x40 / sizeof(uint32_t);
    constexpr size_t REG_UARTICR   = 0x44 / sizeof(uint32_t);

    // Flag Register (UARTFR) bits
    constexpr uint32_t UARTFR_BUSY = 1u << 3;
    constexpr uint32_t UARTFR_RXFE = 1u << 4; // Receive FIFO empty
    constexpr uint32_t UARTFR_TXFF = 1u << 5; // Transmit FIFO full

    // Line Control Register (UARTLCR_H) bits
    constexpr uint32_t UARTLCR_H_FEN    = 1u << 4; // Enable FIFOs
    constexpr uint32_t UARTLCR_H_WLEN_8 = 3u << 5; // 8 bits word length

    // Control Register (UARTCR) bits
    constexpr uint32_t UARTCR_UARTEN = 1u << 0; // UART Enable
    constexpr uint32_t UARTCR_TXE    = 1u << 8; // Transmit Enable
    constexpr uint32_t UARTCR_RXE    = 1u << 9; // Receive Enable

    // Interrupt Mask Set/Clear Register (UARTIMSC) bits
    constexpr uint32_t UARTIMSC_RXIM = 1u << 4; // RX interrupt mask
    constexpr uint32_t UARTIMSC_TXIM = 1u << 5; // TX interrupt mask
    constexpr uint32_t UARTIMSC_RTIM = 1u << 6; // RX timeout interrupt mask
}

void uart::set_interrupt_enable(uint8_t enbl) const {
    if (base == nullptr) {
        return;
    }
    uint32_t imsc = 0;
    if (enbl & 1) {
        imsc |= UARTIMSC_RXIM | UARTIMSC_RTIM;
    }
    if (enbl & 2) {
        imsc |= UARTIMSC_TXIM;
    }
    base[REG_UARTIMSC] = imsc;
}

void uart::disable_interrupts() const {
    if (base == nullptr) {
        return;
    }
    base[REG_UARTIMSC] = 0;
    base[REG_UARTICR] = 0x7FF; // Clear all interrupt flags
}

void uart::set_up() const {
    if (base == nullptr) {
        return;
    }
    disable_interrupts();
    // Disable UART before configuration
    base[REG_UARTCR] = 0;
    // Clear pending interrupts
    base[REG_UARTICR] = 0x7FF;
    // 8 data bits, FIFO enabled, 1 stop bit, no parity
    base[REG_UARTLCR_H] = UARTLCR_H_WLEN_8 | UARTLCR_H_FEN;
    // Enable UART, TX, and RX
    base[REG_UARTCR] = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
}

void uart::output(uint8_t data) const {
    base[REG_UARTDR] = static_cast<uint32_t>(data);
}

uint8_t uart::input() const {
    return static_cast<uint8_t>(base[REG_UARTDR] & 0xFF);
}

bool uart::probe() const {
    if (base == nullptr) {
        if (has_klogger()) {
            get_klogger() << "PL011 UART not available (nullptr base)\n";
        }
        return false;
    }
    set_up();
    if (has_klogger()) {
        get_klogger() << "PL011 UART available\n";
    }
    return true;
}

bool uart::has_data() const {
    if (base == nullptr) {
        return false;
    }
    return (base[REG_UARTFR] & UARTFR_RXFE) == 0;
}

uint8_t uart::read() const {
    while (!has_data()) {
    }
    return input();
}

bool uart::can_write() const {
    if (base == nullptr) {
        return false;
    }
    return (base[REG_UARTFR] & UARTFR_TXFF) == 0;
}

void uart::write(uint8_t data) const {
    while (!can_write()) {
    }
    output(data);
}

uint32_t uart::GetWidth() {
    return 0;
}

uint32_t uart::GetHeight() {
    return 0;
}

void uart::print_at(uint8_t, uint8_t, const char *) {
}

void uart::erase(int, int) {
}

KLogger & uart::operator << (const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            write('\r');
        }
        write(*str);
        ++str;
    }
    return *this;
}

bool uart::has_input() const {
    return false;
}
