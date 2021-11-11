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
public:
    UsbKeycode(usbkbd &kdb, uint8_t keycode);
    ~UsbKeycode() { }
    uint8_t keycode;
};

#define KBREP_BACKLOG_INDEX_MASK 15
#define USBKB_MAX_KEYS           13

#define MOD_STAT_NUMLOCK    1
#define MOD_STAT_CAPSLOCK   2
#define MOD_STAT_SCROLLLOCK 4
#define MOD_STAT_COMPOSE    8
#define MOD_STAT_KANA       16

class usbkbd : public Device {
private:
    UsbIfacedevInformation devInfo;
    std::shared_ptr<usb_endpoint> poll_endpoint;
    std::shared_ptr<usb_transfer> poll_transfer;
    uint64_t transfercount;
    std::thread *kbd_thread;
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
    usbkbd(Bus &bus, UsbIfacedevInformation &devInfo) : Device("usbkbd", &bus), devInfo(devInfo), poll_endpoint(), poll_transfer(), transfercount(0), kbd_thread(nullptr), semaphore(-1),
                                                        kbrep_backlog(), kbrep_windex(0), kbrep_rindex(0), kbrep(), keycodes(), stop(false), mod_status(MOD_STAT_NUMLOCK), modifiers(0) {
        for (int i = 0; i < USBKB_MAX_KEYS; i++) {
            keycodes[i] = nullptr;
        }
    }
    ~usbkbd() override;
    void init() override;
    void submit(uint8_t keycode);
private:
    void interrupt();
    void worker_thread();
};

class usbkbd_driver : public Driver {
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_USBKBD_H
