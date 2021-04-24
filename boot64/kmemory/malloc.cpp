//
// Created by sigsegv on 23.04.2021.
//

#include <new>
#include <strings.h>
#include <core/malloc.h>
#include "mallocator.h"

extern "C" {
    static MemoryAllocator *memoryAllocator = nullptr;

    struct MallocImpl {
        void *(*malloc)(uint32_t);
        void (*free)(void *);
        uint32_t (*sizeof_alloc)(void *);
    };

    static const MallocImpl *impl = nullptr;

    void *wild_malloc(uint32_t size) {
        return memoryAllocator->sm_allocate(size);
    }
    void wild_free(void *ptr) {
        return memoryAllocator->sm_free(ptr);
    }
    uint32_t wild_sizeof_alloc(void *ptr) {
        return memoryAllocator->sm_sizeof(ptr);
    }

    static MallocImpl wild_malloc_struct{};

    void *malloc(uint32_t size) {
        return impl->malloc(size);
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
    void *memoryAllocatorP = (MemoryAllocator *) vpagealloc(0x41000);
    {
        {
            pageentr &pe = get_pageentr64(get_pml4t(), (uint64_t) memoryAllocatorP);
            uint64_t ppage = ppagealloc(4096);
            pe.page_ppn = ppage >> 12;
            pe.writeable = 1;
            pe.execution_disabled = 1;
            pe.accessed = 0;
            pe.present = 1;
        }
        {
            pageentr &pe = get_pageentr64(get_pml4t(), ((uint64_t) memoryAllocatorP) + 4096);
            uint64_t ppage = ppagealloc(4096);
            pe.page_ppn = ppage >> 12;
            pe.writeable = 1;
            pe.execution_disabled = 1;
            pe.accessed = 0;
            pe.present = 1;
        }
    }
    reload_pagetables();
    memoryAllocator = new (memoryAllocatorP) MemoryAllocator();
}

void destroy_simplest_malloc_impl() {
    memoryAllocator->~MemoryAllocator();
    vpagefree((uint64_t) memoryAllocator);
    memoryAllocator = nullptr;
    reload_pagetables();
}
