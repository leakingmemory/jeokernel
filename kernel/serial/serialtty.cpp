//
// Created by sigsegv on 5/17/26.
//

#include "serialtty.h"
#include <sstream>

#include "errno.h"
#include "serialport.h"
#include "../ApStartup.h"
#include "../HardwareInterrupts.h"

serialtty::serialtty(serialport *sport) : Device("tty"),
spinlock(), sport(sport), ioapic(nullptr), lapic(nullptr), outbuf(nullptr), outbuf_size(64), outbuf_off(0), outbuf_len(0) { }

void serialtty::init() {
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Init\n";
        get_klogger() << msg.str().c_str();
    }
    uint8_t ioapic_intn{sport->get_irq()};

    ApStartup *ap = GetApStartup();

    ioapic_intn = ap->GetIsaIoapicPin(ioapic_intn);
    {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": IRQ " << ioapic_intn << "\n";
        get_klogger() << str.str().c_str();
    }

    lapic = ap->GetLocalApic();
    ioapic = ap->GetIoapic();

    /* int pin number + 3 local apic ints below ioapic range */
    get_hw_interrupt_handler().add_handler(ioapic_intn + 3, [this, ioapic_intn] (Interrupt &intr) {
        {
            std::lock_guard lock{spinlock};
            if (outbuf_len > 0 && sport->can_write()) {
                sport->write(*(outbuf + outbuf_off));
                ++outbuf_off;
                --outbuf_len;
                if (outbuf_len == 0) {
                    int_enable = int_enable & 0xFD;
                    sport->set_interrupt_enable(int_enable);
                }
            }
        }
        ioapic->send_eoi(ioapic_intn);
        lapic->eio();
    });

    ioapic->enable(ioapic_intn, lapic->get_lapic_id() >> 24);
    ioapic->send_eoi(ioapic_intn);

    {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": ready\n";
        get_klogger() << str.str().c_str();
    }
}

int serialtty::increase_size(size_t minimum) {
    char *newbuf = reinterpret_cast<char *>(malloc(minimum));
    if (newbuf == nullptr) {
        return -ENOMEM;
    }
    {
        std::lock_guard lock{spinlock};
        if (minimum > outbuf_size || outbuf == nullptr) {
            if (outbuf != nullptr) {
                memcpy(newbuf, outbuf + outbuf_off, outbuf_len);
            }
            char *tmp = outbuf;
            outbuf = newbuf;
            outbuf_size = minimum;
            outbuf_off = 0;
            newbuf = tmp;
        }
    }
    if (newbuf != nullptr) {
        free(newbuf);
    }
    return 0;
}

size_t serialtty::append_buf(const char *output, size_t len) {
    std::lock_guard lock{spinlock};
    if (outbuf == nullptr || len > (outbuf_size - outbuf_len)) {
        auto size = outbuf_size * 2;
        while (len > (size - outbuf_len)) {
            size *= 2;
        }
        return size;
    }
    if (outbuf_off > 0) {
        for (size_t i = 0; i < outbuf_len; i++) {
            outbuf[i] = outbuf[i + outbuf_off];
        }
        outbuf_off = 0;
    }
    memcpy(outbuf + outbuf_len, output, len);
    outbuf_len += len;
    return 0;
}

int serialtty::write(const char *output, size_t len) {
    if (len == 0) {
        return 0;
    }
    do {
        auto need_more = append_buf(output, len);
        if (need_more == 0) {
            break;
        }
        auto err = increase_size(need_more);
        if (err != 0) {
            return err;
        }
    } while (true);
    std::lock_guard lock{spinlock};
    if (outbuf_len > 0 && sport->can_write()) {
        sport->write(*(outbuf + outbuf_off));
        ++outbuf_off;
        --outbuf_len;
        if (outbuf_len > 0) {
            auto ie = int_enable | 2;
            if (ie != int_enable) {
                int_enable = ie;
                sport->set_interrupt_enable(ie);
            }
        }
    }
    return 0;
}

uint32_t serialtty::GetWidth() {
    return 0;
}
uint32_t serialtty::GetHeight() {
    return 0;
}
void serialtty::print_at(uint8_t col, uint8_t row, const char *str) {
}
void serialtty::erase(int backtrack, int erase) {
}
KLogger & serialtty::operator << (const char *str) {
    auto len = strlen(str);
    if (len == 0) {
        return *this;
    }
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            write(str, i);
            char ch[] = {'\r', '\n'};
            write(&(ch[0]), 2);
            return operator <<(str + i + 1);
        }
    }
    write(str, len);
    return *this;
}
