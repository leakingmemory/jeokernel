//
// Created by sigsegv on 11/24/21.
//

#include "UsbBuffer32.h"

UsbBuffer32::~UsbBuffer32() {
}

void *UsbBuffer32::Pointer() {
    return page.Pointer();
}

uint64_t UsbBuffer32::Addr() {
    return page.PhysAddr();
}

size_t UsbBuffer32::Size() {
    return page.Size();
}
