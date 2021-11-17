//
// Created by sigsegv on 10/31/21.
//

#ifndef JEOKERNEL_USBKBD_H
#define JEOKERNEL_USBKBD_H

#include <thread>
#include <devices/devices.h>
#include <devices/drivers.h>
#include <concurrency/raw_semaphore.h>
#include "usbifacedev.h"

struct usbkbd_report {
    uint8_t modifierKeys;
    uint8_t reserved;
    uint8_t keypress[6];

    bool operator == (usbkbd_report &other);
    bool operator != (usbkbd_report &other) {
        return !(*this == other);
    }
    void dump();
} __attribute__ ((__packed__));

class usbkbd;

class UsbKeycode {
private:
    usbkbd &kbd;
public:
    UsbKeycode(usbkbd &kbd, uint32_t modifiers, uint8_t keycode);
    ~UsbKeycode();
    uint64_t rep_ticks;
    uint8_t keycode;
};

#define KBREP_BACKLOG_INDEX_MASK 15
#define USBKB_MAX_KEYS           13
#define USBKB_REPEAT_START_TICKS 50

#define MOD_STAT_NUMLOCK    1
#define MOD_STAT_CAPSLOCK   2
#define MOD_STAT_SCROLLLOCK 4
#define MOD_STAT_COMPOSE    8
#define MOD_STAT_KANA       16

class usbkbd : public Device {
    friend UsbKeycode;
private:
    UsbIfacedevInformation devInfo;
    std::shared_ptr<usb_endpoint> poll_endpoint;
    std::shared_ptr<usb_transfer> poll_transfer;
    uint64_t transfercount;
    std::thread *kbd_thread;
    std::thread *rep_thread;
    std::mutex mtx;
    raw_semaphore semaphore;
    usbkbd_report kbrep_backlog[KBREP_BACKLOG_INDEX_MASK + 1];
    uint8_t kbrep_windex;
    uint8_t kbrep_rindex;
    usbkbd_report kbrep;
    UsbKeycode *keycodes[USBKB_MAX_KEYS];
    bool stop;
    uint8_t mod_status;
    uint8_t modifiers;
public:
    usbkbd(Bus &bus, UsbIfacedevInformation &devInfo) : Device("usbkbd", &bus), devInfo(devInfo), poll_endpoint(), poll_transfer(), transfercount(0), kbd_thread(nullptr), rep_thread(nullptr), mtx(), semaphore(-1),
                                                        kbrep_backlog(), kbrep_windex(0), kbrep_rindex(0), kbrep(), keycodes(), stop(false), mod_status(MOD_STAT_NUMLOCK), modifiers(0) {
        for (int i = 0; i < USBKB_MAX_KEYS; i++) {
            keycodes[i] = nullptr;
        }
    }
    ~usbkbd() override;
    void init() override;
    void submit(uint32_t modifiers, uint8_t keycode);
    uint32_t keyboard_modifiers();
private:
    void set_leds();
    void interrupt();
    void worker_thread();
    void repeat_thread();

    bool ShouldRepeat(uint8_t keycode);
    bool ShouldRelease(uint8_t keycode);
};

class usbkbd_driver : public Driver {
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_USBKBD_H
