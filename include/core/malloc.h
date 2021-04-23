//
// Created by sigsegv on 23.04.2021.
//

#ifndef JEOKERNEL_CORE_MALLOC_H
#define JEOKERNEL_CORE_MALLOC_H

#include <stdint.h>

extern "C" {

    void *malloc(uint32_t size);
    void free(void *);

    void *realloc(void *ptr, uint32_t size);

};

void setup_simplest_malloc_impl();
void destroy_simplest_malloc_impl();

#endif //JEOKERNEL_CORE_MALLOC_H
