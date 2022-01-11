//
// Created by sigsegv on 10/31/21.
//

#include <sstream>
#include "usbkbd.h"
#include "usb_port_connection.h"
#include "usbifacedev.h"
#include "uhid_structs.h"
#include "usb_transfer.h"
#include <keyboard/keyboard.h>

#define USBKB_LEFT_CTRL   1
#define USBKB_LEFT_SHIFT  2
#define USBKB_LEFT_ALT    4
#define USBKB_LEFT_GUI    8
#define USBKB_RIGHT_CTRL  16
#define USBKB_RIGHT_SHIFT 32
#define USBKB_RIGHT_ALT   64
#define USBKB_RIGHT_GUI   128

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

UsbKeycode::UsbKeycode(usbkbd &kbd, uint32_t modifiers, uint8_t keycode) : kbd(kbd), rep_ticks(get_ticks()), keycode(keycode) {
    kbd.submit(modifiers, keycode);
    rep_ticks += USBKB_REPEAT_START_TICKS;
}

UsbKeycode::~UsbKeycode() {
    if (kbd.ShouldRelease(keycode)) {
        kbd.submit(kbd.keyboard_modifiers() | KEYBOARD_CODE_BIT_RELEASE, keycode);
    }
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
    if (!devInfo.port.SetConfigurationValue(devInfo.descr.bConfigurationValue, 0, 0)) {
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

    set_leds();

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
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Endpoint " << endpoint.Address() << "\n";
            get_klogger() << str.str().c_str();
            this->endpoint = endpoint;
            poll_endpoint = devInfo.port.InterruptEndpoint(endpoint.MaxPacketSize(), endpoint.Address(), usb_endpoint_direction::IN, endpoint.bInterval);
            break;
        }
    }

    if (!poll_endpoint) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard lacks a poll endpoint\n";
        get_klogger() << str.str().c_str();
        return;
    }

    poll_transfer = poll_endpoint->CreateTransfer(true, 8, usb_transfer_direction::IN, [this] () {
        this->interrupt();
    }, true, 1, (int8_t) (transfercount++ & 1));

    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": USB";
    {
        auto speed = devInfo.port.Speed();
        switch (speed) {
            case LOW:
                str << " low speed";
                break;
            case FULL:
                str << " full speed";
                break;
            case HIGH:
                str << " high speed";
                break;
            case SUPER:
                str << " super speed";
                break;
            case SUPERPLUS:
                str << " super speed plus";
                break;
        }
    }
    str << " keyboard\n";
    get_klogger() << str.str().c_str();

    kbd_thread = new std::thread([this] () {
        worker_thread();
    });
    rep_thread = new std::thread([this] () {
        repeat_thread();
    });
}

void usbkbd::interrupt() {
    std::shared_ptr<usb_buffer> report = poll_transfer->Buffer();
    if (poll_transfer->IsSuccessful()) {
        report->CopyTo(kbrep_backlog[kbrep_windex++ & KBREP_BACKLOG_INDEX_MASK]);
        semaphore.release();
    } else {
        if (poll_transfer->GetStatus() == usb_transfer_status::STALL || poll_transfer->GetStatus() == usb_transfer_status::DEVICE_NOT_RESPONDING) {
            stalled = true;
            semaphore.release();
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": error " << poll_transfer->GetStatusStr() << "\n";
            get_klogger() << str.str().c_str();
            return;
        }
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": error " << poll_transfer->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
    }
    poll_transfer = poll_endpoint->CreateTransferWithLock(true, 8, usb_transfer_direction::IN, [this] () {
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
    if (rep_thread != nullptr) {
        rep_thread->join();
        delete rep_thread;
        rep_thread = nullptr;
    }
    for (int i = 0; i < USBKB_MAX_KEYS; i++) {
        if (keycodes[i] != nullptr) {
            delete keycodes[i];
            keycodes[i] = nullptr;
        }
    }
}

void usbkbd::worker_thread() {
    auto &kbd = Keyboard();
    while (true) {
        semaphore.acquire();

        if (stalled && !stop) {
            {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Clearing stall condition\n";
                get_klogger() << str.str().c_str();
            }
            auto statusResponse = devInfo.port.ControlRequest(*(devInfo.port.Endpoint0()), usb_get_status(usb_control_endpoint_direction::IN, endpoint.Address()));
            if (statusResponse) {
                uint16_t status{0};
                statusResponse->CopyTo(status);
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Device endpoint status is " << std::hex << status << "\n";
                get_klogger() << str.str().c_str();
            }
            if (devInfo.port.ControlRequest(*(devInfo.port.Endpoint0()), usb_clear_feature(usb_feature::ENDPOINT_HALT, usb_control_endpoint_direction::IN, endpoint.Address()))) {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Cleared device stall condition\n";
                get_klogger() << str.str().c_str();
                transfercount = 0;
            } else {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Clear device stall failed\n";
                get_klogger() << str.str().c_str();
            }
            if (poll_endpoint->ClearStall()) {
                {
                    std::stringstream str{};
                    str << DeviceType() << DeviceId() << ": Cleared host stall condition\n";
                    get_klogger() << str.str().c_str();
                }
            } else {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": Clear host stall failed\n";
                get_klogger() << str.str().c_str();
            }
            stalled = false;
            poll_transfer = poll_endpoint->CreateTransfer(true, 8, usb_transfer_direction::IN, [this] () {
                this->interrupt();
            }, true, 1, (int8_t) (transfercount++ & 1));
            continue;
        }

        std::lock_guard lock{mtx};

        if (stop) {
            break;
        }
        uint32_t kmods{keyboard_modifiers()};
        uint8_t rindex = kbrep_rindex++ & KBREP_BACKLOG_INDEX_MASK;
        if (kbrep != kbrep_backlog[rindex]) {
            kbrep = kbrep_backlog[rindex];
            {
                uint8_t modc = modifiers ^ kbrep.modifierKeys;
                if ((modc & USBKB_LEFT_CTRL) != 0) {
                    uint32_t code{224};
                    if ((kbrep.modifierKeys & USBKB_LEFT_CTRL) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_LCONTROL;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_LEFT_SHIFT) != 0) {
                    uint32_t code{225};
                    if ((kbrep.modifierKeys & USBKB_LEFT_SHIFT) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_LSHIFT;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_LEFT_ALT) != 0) {
                    uint32_t code{226};
                    if ((kbrep.modifierKeys & USBKB_LEFT_ALT) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_LALT;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_LEFT_GUI) != 0) {
                    uint32_t code{227};
                    if ((kbrep.modifierKeys & USBKB_LEFT_GUI) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_LGUI;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_RIGHT_CTRL) != 0) {
                    uint32_t code{228};
                    if ((kbrep.modifierKeys & USBKB_RIGHT_CTRL) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_RCONTROL;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_RIGHT_SHIFT) != 0) {
                    uint32_t code{229};
                    if ((kbrep.modifierKeys & USBKB_RIGHT_SHIFT) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_RSHIFT;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_RIGHT_ALT) != 0) {
                    uint32_t code{230};
                    if ((kbrep.modifierKeys & USBKB_RIGHT_ALT) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_RALT;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
                if ((modc & USBKB_RIGHT_GUI) != 0) {
                    uint32_t code{231};
                    if ((kbrep.modifierKeys & USBKB_RIGHT_GUI) == 0) {
                        code |= KEYBOARD_CODE_BIT_RELEASE;
                        kmods &= ~KEYBOARD_CODE_BIT_RGUI;
                    }
                    code |= kmods;
                    kbd.keycode(code);
                }
            }
            modifiers = kbrep.modifierKeys;
            uint8_t keycodes[USBKB_MAX_KEYS];
            uint8_t num{0};
            if (kbrep.keypress[0] != kbrep.keypress[1] || kbrep.keypress[0] == 0)
            for (int i = 0; i < 6 && kbrep.keypress[i] != 0; i++) {
                keycodes[num++] = kbrep.keypress[i];
            }
            for (int i = 0; i < USBKB_MAX_KEYS; i++) {
                if (this->keycodes[i] != nullptr) {
                    bool found{false};
                    for (int j = 0; j < num; j++) {
                        if (keycodes[j] == this->keycodes[i]->keycode) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        delete this->keycodes[i];
                        this->keycodes[i] = nullptr;
                    }
                }
            }
            for (int i = 0; i < num; i++) {
                bool found{false};
                for (int j = 0; j < USBKB_MAX_KEYS; j++) {
                    if (this->keycodes[j] != nullptr && this->keycodes[j]->keycode == keycodes[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    for (int j = 0; j < USBKB_MAX_KEYS; j++) {
                        if (this->keycodes[j] == nullptr) {
                            this->keycodes[j] = new UsbKeycode(*this, kmods, keycodes[i]);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void usbkbd::submit(uint32_t modifiers, uint8_t keycode) {
    uint32_t fullcode{keycode};
    uint8_t ledc{0};
    switch (keycode) {
        case 83:
            ledc = MOD_STAT_NUMLOCK;
            break;
        case 57:
            ledc = MOD_STAT_CAPSLOCK;
            break;
        case 71:
            ledc = MOD_STAT_SCROLLLOCK;
            break;
    }
    if (ledc != 0) {
        mod_status = mod_status ^ ledc;
        set_leds();
        return;
    }
    fullcode |= modifiers;
    Keyboard().keycode(fullcode);
}

void usbkbd::set_leds() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.ControlRequest(*endpoint0, uhid_set_report(devInfo.iface.bInterfaceNumber, 1), &mod_status)) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB keyboard set LEDs failed\n";
        get_klogger() << str.str().c_str();
        return;
    }
}

uint32_t usbkbd::keyboard_modifiers() {
    uint32_t kmods{0};
    if ((mod_status & MOD_STAT_NUMLOCK) != 0) {
        kmods |= KEYBOARD_CODE_BIT_NUMLOCK;
    }
    if ((mod_status & MOD_STAT_CAPSLOCK) != 0) {
        kmods |= KEYBOARD_CODE_BIT_CAPSLOCK;
    }
    if ((mod_status & MOD_STAT_SCROLLLOCK) != 0) {
        kmods |= KEYBOARD_CODE_BIT_SCROLLLOCK;
    }
    if ((modifiers & USBKB_LEFT_CTRL) != 0) {
        kmods |= KEYBOARD_CODE_BIT_LCONTROL;
    }
    if ((modifiers & USBKB_LEFT_SHIFT) != 0) {
        kmods |= KEYBOARD_CODE_BIT_LSHIFT;
    }
    if ((modifiers & USBKB_LEFT_ALT) != 0) {
        kmods |= KEYBOARD_CODE_BIT_LALT;
    }
    if ((modifiers & USBKB_LEFT_GUI) != 0) {
        kmods |= KEYBOARD_CODE_BIT_LGUI;
    }
    if ((modifiers & USBKB_RIGHT_CTRL) != 0) {
        kmods |= KEYBOARD_CODE_BIT_RCONTROL;
    }
    if ((modifiers & USBKB_RIGHT_SHIFT) != 0) {
        kmods |= KEYBOARD_CODE_BIT_RSHIFT;
    }
    if ((modifiers & USBKB_RIGHT_ALT) != 0) {
        kmods |= KEYBOARD_CODE_BIT_RALT;
    }
    if ((modifiers & USBKB_RIGHT_GUI) != 0) {
        kmods |= KEYBOARD_CODE_BIT_RGUI;
    }
    return kmods;
}

void usbkbd::repeat_thread() {
    auto &kbd = Keyboard();
    while (true) {
        if (stop) {
            break;
        }
        {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(80ms);
        }

        std::lock_guard lock{mtx};

        auto ticks = get_ticks();
        auto mods = keyboard_modifiers();
        for (int j = 0; j < USBKB_MAX_KEYS; j++) {
            if (this->keycodes[j] != nullptr && this->keycodes[j]->rep_ticks < ticks && ShouldRepeat(this->keycodes[j]->keycode)) {
                uint32_t code{mods};
                code |= this->keycodes[j]->keycode;
                kbd.keycode(code);
            }
        }
    }
}

bool usbkbd::ShouldRepeat(uint8_t keycode) {
    switch (keycode) {
        case 83:
        case 57:
        case 71:
            return false;
        default:
            return true;
    }
}

bool usbkbd::ShouldRelease(uint8_t keycode) {
    switch (keycode) {
        case 83:
        case 57:
        case 71:
            return false;
        default:
            return true;
    }
}
