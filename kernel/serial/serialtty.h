//
// Created by sigsegv on 5/17/26.
//

#ifndef JEOKERNEL_SERIALTTY_H
#define JEOKERNEL_SERIALTTY_H

#include <devices/devices.h>
#include <concurrency/hw_spinlock.h>
#include <concepts>
#include "concurrency/raw_semaphore.h"

class serialport;
class IOApic;
class LocalApic;
class serialtty_input_thread;
class keycode_consumer;

constexpr size_t serialtty_inbuf_size = 256;

class serialport_interface {
public:
    virtual uint8_t get_irq() const = 0;
    virtual bool has_data() const = 0;
    virtual uint8_t read() const = 0;
    virtual bool can_write() const = 0;
    virtual void write(uint8_t) const = 0;
    virtual void set_interrupt_enable(uint8_t) const = 0;
};
template <typename sptype> concept SerialportType = requires (sptype sp) {
    { sp.get_irq() } -> std::convertible_to<uint8_t>;
    { sp.has_data() } -> std::convertible_to<bool>;
    { sp.read() } -> std::convertible_to<uint8_t>;
    { sp.can_write() } -> std::convertible_to<bool>;
    { sp.write(std::declval<uint8_t>()) };
    { sp.set_interrupt_enable(std::declval<uint8_t>()) };
};
template <SerialportType sptype> class serialport_pointer : public serialport_interface {
private:
    sptype *sport;
public:
    constexpr serialport_pointer(sptype *sport) : sport(sport) {
    }
    uint8_t get_irq() const override {
        return sport->get_irq();
    }
    bool has_data() const override {
        return sport->has_data();
    }
    uint8_t read() const override {
        return sport->read();
    }
    bool can_write() const override {
        return sport->can_write();
    }
    void write(uint8_t data) const override {
        sport->write(data);
    }
    void set_interrupt_enable(uint8_t enbl) const override {
        sport->set_interrupt_enable(enbl);
    }
};
class serialtty_device;

class serialtty {
    friend class serialtty_device;
private:
    hw_spinlock spinlock;
    std::shared_ptr<serialport_interface> sport;
    IOApic *ioapic;
    LocalApic *lapic;
    std::weak_ptr<serialtty> self_ptr;
    std::string device_type;
    uint32_t device_id;
#if !defined(__aarch64__)
    std::shared_ptr<serialtty_input_thread> input_thread;
#endif
    char *outbuf;
    char inbuf[serialtty_inbuf_size];
    size_t outbuf_size;
    size_t outbuf_off;
    size_t outbuf_len;
    size_t inbuf_off;
    size_t inbuf_len;
    uint8_t int_enable{1};
    serialtty(std::shared_ptr<serialport_interface> &&sport);
public:
    static std::shared_ptr<serialtty> Create(std::shared_ptr<serialport_interface> &&sport);
    template <SerialportType sptype> static std::shared_ptr<serialtty> Create(sptype *sport) {
        std::shared_ptr<serialport_interface> spi = std::make_shared<serialport_pointer<sptype>>(sport);
        return Create(std::move(spi));
    }
    void init(std::string &&device_type, uint32_t device_id);
private:
    int increase_size(size_t minimum);
    size_t append_buf(const char *output, size_t len);
public:
    void consume(std::shared_ptr<keycode_consumer> consumer) const;
    void unconsume(std::shared_ptr<keycode_consumer> consumer) const;
    int write(const char *output, size_t len);
    std::string ReadInputBuffer();
    uint32_t GetWidth();
    uint32_t GetHeight();
    void print_at(uint8_t col, uint8_t row, const char *str);
    void erase(int backtrack, int erase);
    serialtty & operator << (const char *str);

    bool has_input() const;
};

class serialtty_device : public Device {
private:
    std::shared_ptr<serialtty> tty;
public:
    constexpr serialtty_device(const std::shared_ptr<serialtty> &tty) : Device("tty"), tty(tty) {}
    void init() override;
};

class serialtty_klogger : public KLogger {
private:
    std::shared_ptr<serialtty> tty;
public:
    constexpr serialtty_klogger(const std::shared_ptr<serialtty> &tty) : tty(tty) {}
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    void erase(int backtrack, int erase) override;
    KLogger & operator << (const char *str) override;

    std::unique_ptr<keyboard_source_interface> get_keyboard_source() override;
    bool has_input() const override;
};


#endif //JEOKERNEL_SERIALTTY_H
