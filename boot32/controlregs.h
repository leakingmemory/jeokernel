//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_CONTROLREGS_H
#define JEOKERNEL_CONTROLREGS_H

extern "C" {

uint32_t get_cr3();
uint32_t set_cr3(uint32_t cr3);
uint32_t get_cr4();
uint32_t set_cr4(uint32_t cr4);

};

#endif //JEOKERNEL_CONTROLREGS_H
