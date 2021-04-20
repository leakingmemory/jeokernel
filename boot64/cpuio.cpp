//
// Created by sigsegv on 19.04.2021.
//

#include <cpuio.h>

void outportb(uint16_t port, uint8_t data) {
    asm("movb %0, %%al; movw %1, %%dx; out %%al, %%dx" :: "r"(data), "r"(port) : "%al","%dx");
}