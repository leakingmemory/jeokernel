//
// Created by sigsegv on 9/3/26.
//

#include <cstdint>
#include <stdlib.h>

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, uintptr_t size) noexcept {
    ::operator delete(ptr);
}
