//
// Created by sigsegv on 23.04.2021.
//

#ifndef JEOKERNEL_CORE_NEW_H
#define JEOKERNEL_CORE_NEW_H

#include <stdint.h>

void *operator new(long unsigned int size, void*ref) {
    return ref;
}

#endif //JEOKERNEL_CORE_NEW_H
