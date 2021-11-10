//
// Created by sigsegv on 10/31/21.
//

#include <sstream>
#include "usbkbd.h"
#include "usb_port_connection.h"
#include "usbifacedev.h"
#include "uhid_structs.h"
#include "usb_transfer.h"

void usbkbd_report::dump() {
    std::stringstream str{};
    str << "keyboard m="<< std::hex << modifierKeys << " "
        << keypress[0] << " " << keypress[1] << " " << keypress[2] << " "
        << keypress[3] << " " << keypress[4] << " " << keypress[5] << "\n";
    get_klogger() << str.str().c_str();
}

bool usbkbd_report::operator==(usbkbd_report &other) {
    return (
            modifierKeys == other.modifierKeys &&
            keypress[0] == other.keypress[0] &&
            keypress[1] == other.keypress[1] &&
            keypress[2] == other.keypress[2] &&
            keypress[3] == other.keypress[3] &&
            keypress[4] == other.keypress[4] &&
            keypress[5] == other.keypress[5]);
}

Device *usbkbd_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    UsbInterfaceInformation *ifaceInfo{nullptr};
    {
        auto *usbInfo = deviceInformation.GetUsbInformation();
        if (usbInfo != nullptr) {
            ifaceInfo = usbInfo->GetUsbInterfaceInformation();
        }
    }
    if (ifaceInfo != nullptr) {
        auto ifaceDev = ifaceInfo->GetIfaceDev();
        if (
                ifaceInfo->iface.bInterfaceClass == 3 &&
                ifaceInfo->iface.bInterfaceSubClass == 1 &&
                ifaceInfo->iface.bInterfaceProtocol == 1 &&
                ifaceDev != nullptr) {
            auto *dev = new usbkbd(bus, *ifaceDev);
            ifaceDev->ifacedev.SetDevice(*dev);
            return dev;
        }
    }
    return nullptr;
}

void usbkbd::init() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.ControlRequest(*endpoint0, usb_set_configuration(devInfo.descr.bConfigurationValue))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set config failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    if (!devInfo.port.ControlRequest(*endpoint0, uhid_set_protocol(UHID_PROTO_BOOT))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set protocol failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    if (!devInfo.port.ControlRequest(*endpoint0, uhid_set_idle(devInfo.iface.bInterfaceNumber))) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set idle failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    if (!devInfo.port.ControlRequest(*endpoint0, uhid_set_report(devInfo.iface.bInterfaceNumber, 1), &mod_status)) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set LEDs failed\n";
        get_klogger() << str.str().c_str();
        return;
    }

    {
        std::shared_ptr<usb_buffer> report = devInfo.port.ControlRequest(*endpoint0, uhid_get_report());
        if (!report) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Error: USB keyboard test failed\n";
            get_klogger() << str.str().c_str();
            return;
        }
        usbkbd_report kbrep{};
        report->CopyTo(kbrep);
        kbrep.dump();
    }

    for (auto &endpoint : devInfo.endpoints) {
        if (endpoint.IsInterrupt() && endpoint.IsDirectionIn()) {
            poll_endpoint = devInfo.port.InterruptEndpoint(endpoint.MaxPacketSize(), endpoint.Address(), usb_endpoint_direction::IN, endpoint.bInterval);
        }
    }

    if (!poll_endpoint) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard lacks a poll endpoint\n";
        get_klogger() << str.str().c_str();
        return;
    }

    poll_transfer = poll_endpoint->CreateTransfer(8, usb_transfer_direction::IN, [this] () {
        this->interrupt();
    }, true, 1, (int8_t) (transfercount++ & 1));

    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": USB keyboard\n";
    get_klogger() << str.str().c_str();

    kbd_thread = new std::thread([this] () {
        worker_thread();
    });
}

void usbkbd::interrupt() {
    std::shared_ptr<usb_buffer> report = poll_transfer->Buffer();
    report->CopyTo(kbrep_backlog[kbrep_windex++ & KBREP_BACKLOG_INDEX_MASK]);
    semaphore.release();
    poll_transfer = poll_endpoint->CreateTransferWithLock(8, usb_transfer_direction::IN, [this] () {
        this->interrupt();
    }, true, 1, (int8_t) (transfercount++ & 1));
}

usbkbd::~usbkbd() {
    stop = true;
    if (poll_transfer) {
        poll_transfer->SetDoneCall([] () {});
    }
    semaphore.release();
    if (kbd_thread != nullptr) {
        kbd_thread->join();
        delete kbd_thread;
        kbd_thread = nullptr;
    }
}

void usbkbd::worker_thread() {
    while (true) {
        semaphore.acquire();
        if (stop) {
            break;
        }
        uint8_t rindex = kbrep_rindex++ & KBREP_BACKLOG_INDEX_MASK;
        //if (kbrep != kbrep_backlog[rindex]) {
            kbrep = kbrep_backlog[rindex];
            kbrep.dump();
        //}
    }
}
