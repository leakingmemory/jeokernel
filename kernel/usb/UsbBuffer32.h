//
// Created by sigsegv on 11/24/21.
//

#ifndef JEOKERNEL_USBBUFFER32_H
#define JEOKERNEL_USBBUFFER32_H


#include "usb_port_connection.h"
#include "Phys32Page.h"

class UsbBuffer32 : public usb_buffer {
private:
    Phys32Page page;
public:
    UsbBuffer32(size_t size) : page(size) {}
    ~UsbBuffer32() override;
    void *Pointer() override;
    uint64_t Addr() override;
    size_t Size() override;
};


#endif //JEOKERNEL_USBBUFFER32_H
