//
// Created by sigsegv on 6/6/21.
//

#ifndef JEOKERNEL_PS2KBD_H
#define JEOKERNEL_PS2KBD_H

#include "ps2.h"

class ps2kbd : public Device {
private:
    ps2_device_interface &ps2dev;
public:
    ps2kbd(ps2 &ps2, ps2_device_interface &ps2dev) : Device("ps2kbd", &ps2), ps2dev(ps2dev) {}
    void init() override;
};


#endif //JEOKERNEL_PS2KBD_H
