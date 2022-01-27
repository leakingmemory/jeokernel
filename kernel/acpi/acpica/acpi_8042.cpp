//
// Created by sigsegv on 1/25/22.
//

#include <acpi/acpi_8042.h>

extern "C" {
#include <acpi.h>
#include <acpiosxf.h>
#include <acpixf.h>
#include <acobject.h>
#include <acglobal.h>
}
#include <acpi/acpica.h>

acpi_8042::acpi_8042() : keyboard(false), mouse(false) {
    get_acpica().Accept(*this);
}

void acpi_8042::Visit(acpi_device_info &dev_info) {
    std::string hid{};
    if (dev_info.HardwareId.Length > 0) {
        hid = std::string(dev_info.HardwareId.String, dev_info.HardwareId.Length - 1);
    }
    if (hid == "PNP0303") {
        get_klogger() << "ACPI has info about PS/2 keyboard (PNP0303)\n";
        keyboard = true;
    } else if (hid == "PNP0F03") {
        get_klogger() << "ACPI has info about PS/2 mouse (PNP0F03)\n";
        mouse = true;
    } else {
        for (int i = 0; i < dev_info.CompatibleIdList.Count; i++) {
            std::string cid{};
            if (dev_info.CompatibleIdList.Ids[i].Length > 0) {
                cid = std::string(dev_info.CompatibleIdList.Ids[i].String, dev_info.CompatibleIdList.Ids[i].Length - 1);
            }
            if (cid == "PNP0303") {
                std::stringstream str{};
                str << "ACPI has info about PS/2 keyboard (hid=" << hid << ",cid=PNP0303)\n";
                get_klogger() << str.str().c_str();
                keyboard = true;
            } else if (cid == "PNP0F03") {
                std::stringstream str{};
                str << "ACPI has info about PS/2 mouse (hid=" << hid << ",cid=PNP0F03)\n";
                get_klogger() << str.str().c_str();
                mouse = true;
            }
        }
    }
}
