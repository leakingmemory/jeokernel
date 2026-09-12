//
// Created by sigsegv on 23.04.2021.
//

#ifndef JEOKERNEL_CORE_MALLOC_H
#define JEOKERNEL_CORE_MALLOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    void *malloc(size_t size);
    void *calloc(size_t nmemb, size_t size);
    void free_sized(void *, size_t size);
    void *realloc_sized(void *ptr, size_t original_size, size_t new_size);
#if !defined(__aarch64__)
    void free(void *);
    void *realloc(void *ptr, size_t size);
#endif

#ifdef __cplusplus
};

void setup_simplest_malloc_impl();
void setup_simplest_malloc_stats();
void destroy_simplest_malloc_impl();

#endif

#endif //JEOKERNEL_CORE_MALLOC_H
