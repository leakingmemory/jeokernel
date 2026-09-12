//
// Created by sigsegv on 9/3/26.
//

#include <cstdint>
#include <stdlib.h>
#include <new>

#if !defined(__aarch64__)
void operator delete(void* ptr) noexcept {
    free(ptr);
}
#endif

void operator delete(void *ptr, size_t size) noexcept {
    free_sized(ptr, size);
}

void *operator new(size_t size) {
    return malloc(size);
}

extern "C" [[noreturn]] void __cxa_pure_virtual() {
    for (;;) {
        asm volatile("wfe");
    }
}
