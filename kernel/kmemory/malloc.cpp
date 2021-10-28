//
// Created by sigsegv on 23.04.2021.
//

#include <strings.h>
#include <core/malloc.h>
#include <klogger.h>
#include "mallocator.h"
#include "ChainedAllocator.h"

#define MAX_NONPAGED_SIZE 8192

extern "C" {
    static MemoryAllocator *memoryAllocator = nullptr;

    struct MallocImpl {
        void *(*malloc)(uint32_t);
        void (*free)(void *);
        uint32_t (*sizeof_alloc)(void *);
    };

    static const MallocImpl *impl = nullptr;

    void *wild_malloc(uint32_t size) {
        if (size < MAX_NONPAGED_SIZE) {
            return memoryAllocator->sm_allocate(size);
        } else {
            return pagealloc(size);
        }
    }
    void wild_free(void *ptr) {
        if (memoryAllocator->sm_owned(ptr)) {
            return memoryAllocator->sm_free(ptr);
        } else {
            uint64_t vaddr = (uint64_t) ptr;
            if ((vaddr & 0xFFF) != 0) {
                wild_panic("Invalid pagealloc ptr or not ours to free");
            }
            pagefree(ptr);
        }
    }
    uint32_t wild_sizeof_alloc(void *ptr) {
        if (memoryAllocator->sm_owned(ptr)) {
            return memoryAllocator->sm_sizeof(ptr);
        } else {
            uint64_t vaddr = (uint64_t) ptr;
            if ((vaddr & 0xFFF) != 0) {
                wild_panic("Invalid pagealloc ptr or not ours to sizeof");
            }
            return vpagesize(vaddr);
        }
    }

    static MallocImpl wild_malloc_struct{};

    void *malloc(uint32_t size) {
        void *ptr = impl->malloc(size);
        if (ptr == nullptr) {
            wild_panic("Out of memory");
        }
        return ptr;
    }

    void free(void *ptr) {
        impl->free(ptr);
    }

    uint32_t malloc_sizeof(void *ptr) {
        return impl->sizeof_alloc(ptr);
    }

    void *realloc(void *ptr, uint32_t size) {
        uint32_t prev_size = malloc_sizeof(ptr);
        void *newptr = malloc(size);
        if (newptr != nullptr) {
            if (prev_size > size) {
                prev_size = size;
            }
            bcopy(ptr, newptr, prev_size);
            free(ptr);
        }
        return newptr;
    }

};

void setup_simplest_malloc_impl() {
    wild_malloc_struct.malloc = wild_malloc;
    wild_malloc_struct.free = wild_free;
    wild_malloc_struct.sizeof_alloc = wild_sizeof_alloc;
    impl = &wild_malloc_struct;
    memoryAllocator = CreateChainedAllocator();
}

void destroy_simplest_malloc_impl() {
    memoryAllocator->~MemoryAllocator();
}
