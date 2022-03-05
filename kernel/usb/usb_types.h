//
// Created by sigsegv on 1/31/22.
//

#ifndef JEOKERNEL_USB_TYPES_H
#define JEOKERNEL_USB_TYPES_H

enum class usb_transfer_direction {
    SETUP,
    IN,
    OUT
};

enum class usb_endpoint_direction {
    IN,
    OUT,
    BOTH
};

enum usb_speed {
    LOW,
    FULL,
    HIGH,
    SUPER,
    SUPERPLUS
};

enum class usb_endpoint_type {
    CONTROL,
    INTERRUPT,
    BULK
};

#endif //JEOKERNEL_USB_TYPES_H
