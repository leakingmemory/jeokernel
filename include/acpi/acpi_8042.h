//
// Created by sigsegv on 1/25/22.
//

#ifndef JEOKERNEL_ACPI_8042_H
#define JEOKERNEL_ACPI_8042_H

#include <acpi/acpi_visitors.h>

class acpi_8042 : private AcpiDeviceVisitor {
private:
    bool keyboard;
    bool mouse;
public:
    acpi_8042();
    bool has_ps2_keyboard() {
        return keyboard;
    }
    bool has_ps2_mouse() {
        return mouse;
    }
    bool is_dual_channel_ps2() {
        return keyboard && mouse;
    }
private:
    void Visit(acpi_device_info &deviceInfo) override;
};

#endif //JEOKERNEL_ACPI_8042_H
