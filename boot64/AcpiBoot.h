//
// Created by sigsegv on 5/17/21.
//

#ifndef JEOKERNEL_ACPIBOOT_H
#define JEOKERNEL_ACPIBOOT_H


#include <multiboot.h>
#include <thread>

class AcpiBoot {
private:
    std::thread acpi_thread;
public:
    explicit AcpiBoot(const MultibootInfoHeader &multiboot);
    ~AcpiBoot();
private:
    void acpi_boot(const RSDPv1descriptor *rsdp1);
};


#endif //JEOKERNEL_ACPIBOOT_H
