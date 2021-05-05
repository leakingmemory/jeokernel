//
// Created by sigsegv on 19.04.2021.
//

#include <cpuio.h>

void outportb(uint16_t port, uint8_t data) {
    asm("movb %0, %%al; movw %1, %%dx; out %%al, %%dx" :: "r"(data), "r"(port) : "%al","%dx");
}

uint8_t inportb(uint16_t port) {
    uint8_t res;
    asm("movw %1, %%dx; in %%dx, %%al; mov %%al, %0" : "=r"(res) : "r"(port) : "%al", "%dx");
    return res;
}