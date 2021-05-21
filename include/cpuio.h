//
// Created by sigsegv on 19.04.2021.
//

#ifndef JEOKERNEL_CPUIO_H
#define JEOKERNEL_CPUIO_H

#include <stdint.h>

void outportb(uint16_t port, uint8_t data);
uint8_t inportb(uint16_t port);
void outportw(uint16_t port, uint16_t data);
uint16_t inportw(uint16_t port);
void outportl(uint16_t port, uint32_t data);
uint32_t inportl(uint16_t port);

#endif //JEOKERNEL_CPUIO_H
