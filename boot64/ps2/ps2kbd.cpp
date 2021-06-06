//
// Created by sigsegv on 6/6/21.
//

#include <sstream>
#include <klogger.h>
#include "ps2kbd.h"

void ps2kbd::init() {
    std::stringstream msg;
    msg << DeviceType() << (unsigned int) DeviceId() << ": Init\n";
    get_klogger() << msg.str().c_str();
}
