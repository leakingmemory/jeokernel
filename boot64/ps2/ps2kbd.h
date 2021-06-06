//
// Created by sigsegv on 6/6/21.
//

#ifndef JEOKERNEL_PS2KBD_H
#define JEOKERNEL_PS2KBD_H

#include "ps2.h"
#include "../IOApic.h"
#include <core/LocalApic.h>
#include <memory>

class ps2kbd : public Device {
private:
    ps2_device_interface &ps2dev;
    std::unique_ptr<IOApic> ioapic;
    std::unique_ptr<LocalApic> lapic;
public:
    ps2kbd(ps2 &ps2, ps2_device_interface &ps2dev) : Device("ps2kbd", &ps2), ps2dev(ps2dev), ioapic(), lapic() {}
    void init() override;
};


#endif //JEOKERNEL_PS2KBD_H
