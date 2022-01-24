//
// Created by sigsegv on 10/17/21.
//

#include "usb_transfer.h"

std::string usb_transfer::GetStatusStr() {
    auto status = GetStatus();
    switch (status) {
        case usb_transfer_status::NO_ERROR:
            return "No error";
        case usb_transfer_status::CRC:
            return "CRC";
        case usb_transfer_status::BIT_STUFFING:
            return "Bit stuffing";
        case usb_transfer_status::DATA_TOGGLE_MISMATCH:
            return "Data toggle mismatch";
        case usb_transfer_status::STALL:
            return "Stall";
        case usb_transfer_status::DEVICE_NOT_RESPONDING:
            return "Device not responding";
        case usb_transfer_status::PID_CHECK_FAILURE:
            return "PID check failure";
        case usb_transfer_status::UNEXPECTED_PID:
            return "Unexpected PID";
        case usb_transfer_status::DATA_OVERRUN:
            return "Data overrun";
        case usb_transfer_status::DATA_UNDERRUN:
            return "Data underrun";
        case usb_transfer_status::BUFFER_OVERRUN:
            return "Buffer overrun";
        case usb_transfer_status::BUFFER_UNDERRUN:
            return "Buffer underrun";
        case usb_transfer_status::NOT_ACCESSED:
            return "Not accessed";
        case usb_transfer_status::UNKNOWN_ERROR:
            return "Unknown error";
        default:
            return "Unknown error code";
    }
}

void usb_transfer::SetDone() {
    if (!done) {
        done = true;
        doneCall();
    }
}
