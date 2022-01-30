//
// Created by sigsegv on 1/30/22.
//

#include "usbhub.h"
#include "sstream"

Device *usbhub_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (deviceInformation.device_class == 9) {
        auto *usbInfo = deviceInformation.GetUsbInformation();
        if (usbInfo != nullptr) {
            auto *dev = new usbhub(bus, *usbInfo);
            usbInfo->port.SetDevice(*dev);
            return dev;
        }
    }
    return nullptr;
}

usbhub::~usbhub() {

}

void usbhub::init() {
    std::shared_ptr<usb_endpoint> endpoint0 = usbDeviceInformation.port.Endpoint0();
    {
        auto buffer = usbDeviceInformation.port.ControlRequest(*endpoint0, usb_req_hubdesc(sizeof(descr)));
        if (!buffer) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": failed to get hub descriptor\n";
            get_klogger() << str.str().c_str();
        }
        buffer->CopyTo(descr);
    }
    {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": ports=" << descr.portCount << " type=" << descr.type << " flags=" << descr.flags << " port-time=" << descr.portPowerTime << " current=" << descr.current << "\n";
        get_klogger() << str.str().c_str();
    }
}
