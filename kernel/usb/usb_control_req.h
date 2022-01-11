//
// Created by sigsegv on 10/11/21.
//

#ifndef JEOKERNEL_USB_CONTROL_REQ_H
#define JEOKERNEL_USB_CONTROL_REQ_H

#include <stdint.h>

enum class usb_control_direction {
    HOST_TO_DEVICE, DEVICE_TO_HOST
};

enum class usb_control_request_type {
    STANDARD, CLASS, VENDOR
};

enum class usb_control_endpoint_direction {
    IN, OUT
};

enum class usb_control_recepient {
    DEVICE, INTERFACE, ENDPOINT, OTHER
};

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

    constexpr uint8_t RequestType(usb_control_direction dir, usb_control_request_type req_type, usb_control_recepient recip) {
        uint8_t reqType{0};
        if (dir == usb_control_direction::DEVICE_TO_HOST) {
            reqType = 0x80;
        }
        switch (req_type) {
            case usb_control_request_type::STANDARD:
                break;
            case usb_control_request_type::CLASS:
                reqType |= 0x20;
                break;
            case usb_control_request_type::VENDOR:
                reqType |= 0x40;
                break;
            default:
                reqType |= 0x60;
        }
        switch (recip) {
            case usb_control_recepient::DEVICE:
                break;
            case usb_control_recepient::INTERFACE:
                reqType |= 1;
                break;
            case usb_control_recepient::ENDPOINT:
                reqType |= 2;
                break;
            default:
                reqType |= 3;
        }
        return reqType;
    }
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

struct usb_set_configuration : usb_control_request {
    usb_set_configuration(uint8_t configValue)
    : usb_control_request(0, 9, configValue, 0, 0) { }
} __attribute__((__packed__));

struct usb_set_interface : usb_control_request {
    usb_set_interface(uint8_t interface, uint8_t alternateSetting)
    : usb_control_request(1, 11, alternateSetting, interface, 0) { }
};

enum class usb_feature {
    ENDPOINT_HALT, DEVICE_REMOTE_WAKEUP, TEST_MODE
};

struct usb_get_status : usb_control_request {
    usb_get_status(usb_control_recepient recip, uint8_t id)
    : usb_control_request(RequestType(usb_control_direction::DEVICE_TO_HOST, usb_control_request_type::STANDARD, recip), 0, 0, id, 2) { }
    usb_get_status(usb_control_endpoint_direction dir, uint8_t endpoint)
    : usb_get_status(usb_control_recepient::ENDPOINT, Endpoint(dir, endpoint)) { }

    constexpr uint16_t Endpoint(usb_control_endpoint_direction dir, uint8_t endpoint) {
        uint16_t id{endpoint};
        if (dir == usb_control_endpoint_direction::IN) {
            id |= 0x80;
        }
        return id;
    }
};

struct usb_clear_feature : usb_control_request {
    usb_clear_feature(usb_control_recepient recip, usb_feature feature, uint8_t id)
            : usb_control_request(RequestType(usb_control_direction::HOST_TO_DEVICE, usb_control_request_type::STANDARD, recip), 1, Selector(feature), id, 0) { }
    usb_clear_feature(usb_feature feature, usb_control_endpoint_direction dir, uint8_t endpoint)
            : usb_clear_feature(usb_control_recepient::ENDPOINT, feature, Endpoint(dir, endpoint)) { }

    constexpr uint8_t Selector(usb_feature feature) {
        switch (feature) {
            case usb_feature::ENDPOINT_HALT:
                return 0;
            case usb_feature::DEVICE_REMOTE_WAKEUP:
                return 1;
            case usb_feature::TEST_MODE:
                return 2;
            default:
                return 0xFF;
        }
    }
    constexpr uint16_t Endpoint(usb_control_endpoint_direction dir, uint8_t endpoint) {
        uint16_t id{endpoint};
        if (dir == usb_control_endpoint_direction::IN) {
            id |= 0x80;
        }
        return id;
    }
};

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

struct usb_configuration_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
}  __attribute__((__packed__));

struct usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
}  __attribute__((__packed__));

struct usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;

    bool IsDirectionIn() {
        return (bEndpointAddress & 0x80) != 0;
    }
    uint8_t Address() {
        return bEndpointAddress & 0xF;
    }
    bool IsInterrupt() {
        return (bmAttributes & 3) == 3;
    }
    uint16_t MaxPacketSize() {
        return wMaxPacketSize & 0x3FF;
    }
}  __attribute__((__packed__));

#endif //JEOKERNEL_USB_CONTROL_REQ_H
