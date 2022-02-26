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

enum class usb_control_request_num {
    GET_STATUS, CLEAR_FEATURE, SET_FEATURE, SET_ADDRESS, GET_DESCRIPTOR,
    SET_DESCRIPTOR, GET_CONFIGURATION, SET_CONFIGURATION, GET_INTERFACE,
    SET_INTERFACE
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

    constexpr static uint8_t RequestType(usb_control_direction dir, usb_control_request_type req_type, usb_control_recepient recip) {
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
    constexpr static uint8_t Request(usb_control_request_num req) {
        switch (req) {
            case usb_control_request_num::GET_STATUS:
                return 0;
            case usb_control_request_num::CLEAR_FEATURE:
                return 1;
            case usb_control_request_num::SET_FEATURE:
                return 3;
            case usb_control_request_num::SET_ADDRESS:
                return 5;
            case usb_control_request_num::GET_DESCRIPTOR:
                return 6;
            case usb_control_request_num::SET_DESCRIPTOR:
                return 7;
            case usb_control_request_num::GET_CONFIGURATION:
                return 8;
            case usb_control_request_num::SET_CONFIGURATION:
                return 9;
            case usb_control_request_num::GET_INTERFACE:
                return 10;
            case usb_control_request_num::SET_INTERFACE:
                return 11;
        }
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
    : usb_control_request(0x80, Request(usb_control_request_num::GET_DESCRIPTOR), (descriptorType << 8) + descriptorIndex, languageId, descriptorLength) { }
} __attribute__((__packed__));

struct usb_set_configuration : usb_control_request {
    usb_set_configuration(uint8_t configValue)
    : usb_control_request(0, Request(usb_control_request_num::SET_CONFIGURATION), configValue, 0, 0) { }
} __attribute__((__packed__));

struct usb_set_interface : usb_control_request {
    usb_set_interface(uint8_t interface, uint8_t alternateSetting)
    : usb_control_request(1, Request(usb_control_request_num::SET_INTERFACE), alternateSetting, interface, 0) { }
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

    bool IsDirectionIn() const {
        return (bEndpointAddress & 0x80) != 0;
    }
    uint8_t Address() const {
        return bEndpointAddress & 0xF;
    }
    bool IsInterrupt() const {
        return (bmAttributes & 3) == 3;
    }
    bool IsBulk() const {
        return (bmAttributes & 3) == 2;
    }
    uint16_t MaxPacketSize() const {
        return wMaxPacketSize & 0x3FF;
    }
}  __attribute__((__packed__));

struct usb_req_hubdesc : usb_control_request {
    usb_req_hubdesc(uint16_t length) : usb_control_request(
            RequestType(usb_control_direction::DEVICE_TO_HOST, usb_control_request_type::CLASS, usb_control_recepient::DEVICE),
            Request(usb_control_request_num::GET_DESCRIPTOR), 0x2900, 0, length
            ) { }
};

struct usb_req_hubportstatus : usb_control_request {
    usb_req_hubportstatus(uint16_t port) : usb_control_request(
            RequestType(usb_control_direction::DEVICE_TO_HOST, usb_control_request_type::CLASS, usb_control_recepient::OTHER),
            Request(usb_control_request_num::GET_STATUS), 0, port + 1, 4
            ) { }
};

enum class usb_hub_port_feature {
    ENABLE, RESET, POWER, CONNECTION_CHANGE, ENABLE_CHANGE, SUSPEND_CHANGE, OVERCURRENT_CHANGE, RESET_CHANGE
};
struct usb_req_port_feature : usb_control_request {
    usb_req_port_feature(uint16_t port, uint8_t request, usb_hub_port_feature feature) : usb_control_request(
            RequestType(usb_control_direction::HOST_TO_DEVICE, usb_control_request_type::CLASS, usb_control_recepient::OTHER),
            request, PortFeature(feature), port + 1, 0
            ) { }
    constexpr static uint16_t PortFeature(usb_hub_port_feature feature) {
        switch (feature) {
            case usb_hub_port_feature::ENABLE:
                return 1;
            case usb_hub_port_feature::RESET:
                return 4;
            case usb_hub_port_feature::POWER:
                return 8;
            case usb_hub_port_feature::CONNECTION_CHANGE:
                return 16;
            case usb_hub_port_feature::ENABLE_CHANGE:
                return 17;
            case usb_hub_port_feature::SUSPEND_CHANGE:
                return 18;
            case usb_hub_port_feature::OVERCURRENT_CHANGE:
                return 19;
            case usb_hub_port_feature::RESET_CHANGE:
                return 20;
        }
    }
};
struct usb_req_set_port_feature : usb_req_port_feature {
    usb_req_set_port_feature(uint16_t port, usb_hub_port_feature feature) : usb_req_port_feature(
            port, Request(usb_control_request_num::SET_FEATURE), feature
            ) { }
};
struct usb_req_clear_port_feature : usb_req_port_feature {
    usb_req_clear_port_feature(uint16_t port, usb_hub_port_feature feature) : usb_req_port_feature(
            port, Request(usb_control_request_num::CLEAR_FEATURE), feature
    ) { }
};

#endif //JEOKERNEL_USB_CONTROL_REQ_H
