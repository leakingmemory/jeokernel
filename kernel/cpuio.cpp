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

void outportw(uint16_t port, uint16_t data) {
    asm("movw %0, %%ax; movw %1, %%dx; outw %%ax, %%dx" :: "r"(data), "r"(port) : "%ax","%dx");
}

uint16_t inportw(uint16_t port) {
    uint16_t res;
    asm("movw %1, %%dx; inw %%dx, %%ax; movw %%ax, %0" : "=r"(res) : "r"(port) : "%ax", "%dx");
    return res;
}

void outportl(uint16_t port, uint32_t data) {
    asm("movl %0, %%eax; movw %1, %%dx; outl %%eax, %%dx" :: "r"(data), "r"(port) : "%eax","%dx");
}

uint32_t inportl(uint16_t port) {
    uint32_t res;
    asm("movw %1, %%dx; inl %%dx, %%eax; movl %%eax, %0" : "=r"(res) : "r"(port) : "%eax", "%dx");
    return res;
}
