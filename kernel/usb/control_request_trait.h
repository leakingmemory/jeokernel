//
// Created by sigsegv on 12/11/21.
//

#ifndef JEOKERNEL_CONTROL_REQUEST_TRAIT_H
#define JEOKERNEL_CONTROL_REQUEST_TRAIT_H


#include <memory>

struct usb_control_request;
class usb_endpoint;
class usb_buffer;

class control_request_trait {
public:
    std::shared_ptr<usb_buffer> ControlRequest(usb_endpoint &endpoint, const usb_control_request &request);
    bool ControlRequest(usb_endpoint &endpoint, const usb_control_request &request, void *data);
};


#endif //JEOKERNEL_CONTROL_REQUEST_TRAIT_H
