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

#define KBREP_BACKLOG_INDEX_MASK 1

class usbkbd : public Device {
private:
    UsbIfacedevInformation devInfo;
    std::shared_ptr<usb_endpoint> poll_endpoint;
    std::shared_ptr<usb_transfer> poll_transfer;
    std::thread *kbd_thread;
    raw_semaphore semaphore;
    usbkbd_report kbrep_backlog[KBREP_BACKLOG_INDEX_MASK + 1];
    uint8_t kbrep_windex;
    uint8_t kbrep_rindex;
    usbkbd_report kbrep;
    bool stop;
public:
    usbkbd(Bus &bus, UsbIfacedevInformation &devInfo) : Device("usbkbd", &bus), devInfo(devInfo), poll_endpoint(), poll_transfer(), kbd_thread(nullptr), semaphore(-1), kbrep_backlog(), kbrep_windex(0), kbrep_rindex(0), kbrep(), stop(false) {
    }
    ~usbkbd() override;
    void init() override;
private:
    void interrupt();
    void worker_thread();
};

class usbkbd_driver : public Driver {
    Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_USBKBD_H
