//
// Created by sigsegv on 10/17/21.
//

#ifndef JEOKERNEL_USB_TRANSFER_H
#define JEOKERNEL_USB_TRANSFER_H

#include <memory>
#include <string>

enum class usb_transfer_status {
        NO_ERROR,
        CRC,
        BIT_STUFFING,
        DATA_TOGGLE_MISMATCH,
        STALL,
        DEVICE_NOT_RESPONDING,
        PID_CHECK_FAILURE,
        UNEXPECTED_PID,
        DATA_OVERRUN,
        DATA_UNDERRUN,
        BUFFER_OVERRUN,
        BUFFER_UNDERRUN,
        NOT_ACCESSED,
        UNKNOWN_ERROR
};

class usb_buffer;

class usb_transfer {
private:
    bool done;
public:
    usb_transfer() : done(false) {}
    virtual ~usb_transfer() {}
    virtual std::shared_ptr<usb_buffer> Buffer() = 0;
    virtual void SetDone() { done = true;}
    bool IsDone() { return done; }
    virtual usb_transfer_status GetStatus() = 0;
    std::string GetStatusStr();
    bool IsSuccessful() {
        return done && GetStatus() == usb_transfer_status::NO_ERROR;
    }
};

#endif //JEOKERNEL_USB_TRANSFER_H
