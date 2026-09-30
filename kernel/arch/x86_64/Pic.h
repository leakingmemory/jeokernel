//
// Created by sigsegv on 02.05.2021.
//

#ifndef JEOKERNEL_PIC_H
#define JEOKERNEL_PIC_H


#include <cstdint>

class Pic {
public:
    void remap(uint8_t vec_base);
    void disable();
};


#endif //JEOKERNEL_PIC_H
