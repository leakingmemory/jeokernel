//
// Created by sigsegv on 10/11/21.
//

#ifndef JEOKERNEL_USB_CONTROL_REQ_H
#define JEOKERNEL_USB_CONTROL_REQ_H

#include <stdint.h>

struct usb_control_request {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;

    usb_control_request(
                uint8_t bmRequestType,
                uint8_t bRequest,
                uint16_t wValue,
                uint16_t wIndex,
                uint16_t wLength
            ) :
            bmRequestType(bmRequestType),
            bRequest(bRequest),
            wValue(wValue),
            wIndex(wIndex),
            wLength(wLength) {}
} __attribute__((__packed__));

struct usb_set_address : usb_control_request {
    usb_set_address(uint8_t addr) : usb_control_request(0, 5, addr, 0, 0) {}
} __attribute__((__packed__));

#define DESCRIPTOR_TYPE_DEVICE                      1
#define DESCRIPTOR_TYPE_CONFIGURATION               2
#define DESCRIPTOR_TYPE_STRING                      3
#define DESCRIPTOR_TYPE_INTERFACE                   4
#define DESCRIPTOR_TYPE_ENDPOINT                    5
#define DESCRIPTOR_TYPE_DEVICE_QUALIFIER            6
#define DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION   7
#define DESCRIPTOR_TYPE_INTERFACE_POWER             8
struct usb_get_descriptor : usb_control_request {
    usb_get_descriptor(uint8_t descriptorType, uint8_t descriptorIndex, uint8_t descriptorLength, uint8_t languageId = 0)
    : usb_control_request(0x80, 6, (descriptorType << 8) + descriptorIndex, languageId, descriptorLength) { }
} __attribute__((__packed__));

struct usb_minimum_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
} __attribute__((__packed__));

static_assert(sizeof(usb_minimum_device_descriptor) == 8);

struct usb_device_descriptor : usb_minimum_device_descriptor {
    uint16_t idVector;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
}  __attribute__((__packed__));

static_assert(sizeof(usb_device_descriptor) == 0x12);


#endif //JEOKERNEL_USB_CONTROL_REQ_H
