//
// Created by sigsegv on 5/17/26.
//

#include "serialtty.h"
#include <sstream>
#include <thread>

#include "errno.h"
#include "serialport.h"
#include "../ApStartup.h"
#include "../HardwareInterrupts.h"
#include <keyboard/keyboard.h>

class serialtty_input_thread {
private:
    hw_spinlock mtx;
    raw_semaphore semaphore;
    std::weak_ptr<serialtty> stty;
    std::shared_ptr<keycode_consumer> consumer;
    std::vector<std::shared_ptr<keycode_consumer>> consumers;
public:
    serialtty_input_thread(const std::shared_ptr<serialtty> &stty) : mtx(), semaphore(-1), stty(stty) {
    }
    void init(std::shared_ptr<serialtty_input_thread> stty) {
        std::thread thr{[stty] () mutable {
            stty->run(std::move(stty));
        }};
        thr.detach();
    }
    void signal() {
        semaphore.release();
    }
    void consume(std::shared_ptr<keycode_consumer> consumer);
    void unconsume(std::shared_ptr<keycode_consumer> consumer);
    static void run(std::shared_ptr<serialtty_input_thread> &&stty_input);
};

void serialtty_input_thread::consume(std::shared_ptr<keycode_consumer> consumer) {
    std::lock_guard lck{mtx};
    consumers.push_back(consumer);
}
void serialtty_input_thread::unconsume(std::shared_ptr<keycode_consumer> consumer) {
    std::lock_guard lck{mtx};
    if (consumer == this->consumer) {
        this->consumer = {};
        return;
    }
    auto iterator = consumers.begin();
    while (iterator != consumers.end()) {
        if (consumer == *iterator) {
            consumers.erase(iterator);
            return;
        }
        ++iterator;
    }
}

void serialtty_input_thread::run(std::shared_ptr<serialtty_input_thread> &&stty_input) {
    std::shared_ptr<serialtty_input_thread> stty{std::move(stty_input)};
    while (true) {
        stty->semaphore.acquire();
        auto tty = stty->stty.lock();
        if (!tty) {
            break;
        }
        auto buf = tty->ReadInputBuffer();
        if (buf.empty()) {
            continue;
        }
        std::shared_ptr<keycode_consumer> consumer;
        {
            std::lock_guard lock{stty->mtx};
            consumer = stty->consumer;
            if (!consumer) {
                auto iterator = stty->consumers.begin();
                if (iterator != stty->consumers.end()) {
                    stty->consumer = *iterator;
                    stty->consumers.erase(iterator);
                    consumer = stty->consumer;
                }
            }
        }
        for (const char ch : buf) {
            if (consumer) {
                if (consumer->IsRawModeSupported()) {
                    auto continueSending = consumer->Consume(KEYCODE_CONSUMER_RAW_MODE, ch);
                    if (!continueSending) {
                        std::lock_guard lock{stty->mtx};
                        if (stty->consumer == consumer) {
                            stty->consumer = {};
                        }
                        consumer = stty->consumer;
                        if (!consumer) {
                            auto iterator = stty->consumers.begin();
                            if (iterator != stty->consumers.end()) {
                                stty->consumer = *iterator;
                                stty->consumers.erase(iterator);
                                consumer = stty->consumer;
                            }
                        }
                    }
                }
            }
        }
    }
}

serialtty::serialtty(serialport *sport) : spinlock(), sport(sport), ioapic(nullptr), lapic(nullptr), outbuf(nullptr), outbuf_size(64), outbuf_off(0), outbuf_len(0), inbuf_off(0), inbuf_len(0) { }

std::shared_ptr<serialtty> serialtty::Create(serialport *sport) {
    std::shared_ptr<serialtty> shptr{new serialtty(sport)};
    std::weak_ptr<serialtty> wptr{shptr};
    shptr->self_ptr = std::move(wptr);
    return shptr;
}

void serialtty::init(std::string &&i_device_type, uint32_t i_device_id) {
    device_type = std::move(i_device_type);
    device_id = i_device_id;
    {
        std::stringstream msg;
        msg << device_type << (unsigned int) device_id << ": Init\n";
        get_klogger() << msg.str().c_str();
    }
    uint8_t ioapic_intn{sport->get_irq()};

    ApStartup *ap = GetApStartup();

    ioapic_intn = ap->GetIsaIoapicPin(ioapic_intn);
    {
        std::stringstream str{};
        str << device_type << device_id << ": IRQ " << ioapic_intn << "\n";
        get_klogger() << str.str().c_str();
    }

    lapic = ap->GetLocalApic();
    ioapic = ap->GetIoapic();

    /* int pin number + 3 local apic ints below ioapic range */
    get_hw_interrupt_handler().add_handler(ioapic_intn + 3, [this, ioapic_intn] (Interrupt &intr) {
        bool signal_input;
        {
            std::lock_guard lock{spinlock};
            while (sport->has_data()) {
                auto ch = sport->read();
                if (inbuf_off > 0) {
                    for (size_t i = 0; i < inbuf_len; i++) {
                        inbuf[i] = inbuf[i + inbuf_off];
                    }
                    inbuf_off = 0;
                }
                if (inbuf_len < serialtty_inbuf_size) {
                    inbuf[inbuf_len++] = ch;
                } else {
                    break;
                }
            }
            signal_input = inbuf_len > 0;
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
        if (signal_input) {
            input_thread->signal();
        }
        ioapic->send_eoi(ioapic_intn);
        lapic->eio();
    });

    ioapic->enable(ioapic_intn, lapic->get_lapic_id() >> 24);
    ioapic->send_eoi(ioapic_intn);

    sport->set_interrupt_enable(int_enable);

    input_thread = std::make_shared<serialtty_input_thread>(self_ptr.lock());
    input_thread->init(input_thread);

    {
        std::stringstream str{};
        str << device_type << device_id << ": ready\n";
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

void serialtty::consume(std::shared_ptr<keycode_consumer> consumer) const {
    input_thread->consume(consumer);
}

void serialtty::unconsume(std::shared_ptr<keycode_consumer> consumer) const {
    input_thread->unconsume(consumer);
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

std::string serialtty::ReadInputBuffer() {
    char buf[serialtty_inbuf_size];
    size_t len;
    {
        std::lock_guard lock{spinlock};
        len = inbuf_len;
        memcpy(buf, inbuf + inbuf_off, len);
        inbuf_off = 0;
        inbuf_len = 0;
    }
    return {buf, len};
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
    std::string str{};
    if (backtrack < erase) {
        str.reserve(backtrack + (erase * 2) + 1);
        for (int i = 0; i < backtrack; ++i) {
            str.append("\10 \10");
        }
        std::string sp{};
        sp.reserve((erase - backtrack ) * 2 + 1);
        for (int i = 0; i < (erase - backtrack); ++i) {
            str.append("\10");
            sp.append(" ");
        }
        str.append(sp);
    } else {
        str.reserve((erase * 2) + backtrack + 1);
        for (int i = 0; i < erase; ++i) {
            str.append("\10 \10");
        }
        for (int i = 0; i < (backtrack - erase); ++i) {
            str.append("\10");
        }
    }
    write(str.c_str(), str.size());
}
serialtty & serialtty::operator << (const char *str) {
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

bool serialtty::has_input() const {
    return true;
}

void serialtty_device::init() {
    auto device_type = DeviceType();
    tty->init(std::move(device_type), DeviceId());
}

uint32_t serialtty_klogger::GetWidth() {
    return tty->GetWidth();
}
uint32_t serialtty_klogger::GetHeight() {
    return tty->GetHeight();
}
void serialtty_klogger::print_at(uint8_t col, uint8_t row, const char *str) {
    tty->print_at(col, row, str);
}
void serialtty_klogger::erase(int backtrack, int erase) {
    tty->erase(backtrack, erase);
}
KLogger & serialtty_klogger::operator << (const char *str) {
    tty->operator <<(str);
    return *this;
}

std::unique_ptr<keyboard_source_interface> serialtty_klogger::get_keyboard_source() {
    std::unique_ptr<keyboard_source_interface> ksi{new keyboard_source_interface_impl(tty) };
    return ksi;
}

bool serialtty_klogger::has_input() const {
    return tty->has_input();
}
