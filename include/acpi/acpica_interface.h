//
// Created by sigsegv on 1/30/22.
//

#ifndef JEOKERNEL_ACPICA_INTERFACE_H
#define JEOKERNEL_ACPICA_INTERFACE_H

class acpica_interface {
public:
    virtual bool reboot() = 0;
    virtual bool poweroff() = 0;
};

acpica_interface &get_acpica_interface();

#endif //JEOKERNEL_ACPICA_INTERFACE_H
