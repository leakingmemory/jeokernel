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
    bool has8042;
public:
    static const RSDPv1descriptor *GetRsdpPtrFromMultiboot(const MultibootInfoHeader &multiboot);
    explicit AcpiBoot(const RSDPv1descriptor *rsdp1);
    ~AcpiBoot();
private:
    void acpi_boot(const RSDPv1descriptor *rsdp1);
public:
    void join();
    bool has_8042();
};


#endif //JEOKERNEL_ACPIBOOT_H
