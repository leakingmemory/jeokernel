//
// Created by sigsegv on 10/30/21.
//

#include "usbifacedev.h"

void usbifacedev::init() {
    usbDeviceInformation.port.ReadConfigurations();
}
