//
// Created by sigsegv on 6/6/21.
//

#ifndef JEOKERNEL_PS2KBD_H
#define JEOKERNEL_PS2KBD_H

#include "ps2.h"
#include "../IOApic.h"
#include <core/LocalApic.h>
#include <memory>
#include <concurrency/hw_spinlock.h>
#include <concurrency/raw_semaphore.h>

#define RD_RING_BUFSIZE_BITS 4
#define RD_RING_BUFSIZE      (1 << RD_RING_BUFSIZE_BITS)
#define RD_RING_BUFSIZE_MASK (RD_RING_BUFSIZE - 1)

class ps2kbd : public Device {
private:
    hw_spinlock spinlock;
    raw_semaphore sema;
    bool stop;
    std::thread extractor_thread;
    ps2_device_interface &ps2dev;
    std::unique_ptr<IOApic> ioapic;
    std::unique_ptr<LocalApic> lapic;
    uint8_t buffer[RD_RING_BUFSIZE];
    uint32_t inspos, outpos;
public:
    ps2kbd(ps2 &ps2, ps2_device_interface &ps2dev) : Device("ps2kbd", &ps2),
    spinlock(), sema(-1), stop(false), extractor_thread([this] () { this->extractor(); }),
    ps2dev(ps2dev), ioapic(), lapic(), buffer(), inspos(0), outpos(0) {}
    ~ps2kbd() {
        {
            critical_section cli{};
            std::lock_guard lock(spinlock);
            stop = true;
            sema.release();
        }
        extractor_thread.join();
    }
    void init() override;
    void extractor();
};


#endif //JEOKERNEL_PS2KBD_H
