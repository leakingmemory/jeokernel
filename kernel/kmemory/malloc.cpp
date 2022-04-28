//
// Created by sigsegv on 23.04.2021.
//

#include <strings.h>
#include <core/malloc.h>
#include <klogger.h>
#include "mallocator.h"
#include "ChainedAllocatorRoot.h"
#include <kernelconfig.h>
#include <string>
#include <concurrency/critical_section.h>
#include <mutex>
#include <new>
#include <stats/statistics_root.h>

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
            if (memoryAllocator->sm_free(ptr) == 0) {
                get_klogger() << "Free: Invalid ptr or not ours to free\n";
            }
        } else {
            uint64_t vaddr = (uint64_t) ptr;
            if ((vaddr & 0xFFF) != 0) {
                get_klogger() << "Free: Invalid pagealloc ptr or not ours to free\n";
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

    uint64_t wild_malloc_total() {
        return memoryAllocator->sm_total_size();
    }

    uint64_t wild_malloc_allocated() {
        return memoryAllocator->sm_allocated_size();
    }

    static MallocImpl wild_malloc_struct{};

    void *malloc(uint32_t size) {
        void *ptr = impl->malloc(size);
        if (ptr == nullptr) {
            wild_panic("Out of memory");
        }
#ifdef MALLOC_INIT_POISON
        memset(ptr, 0xDE, size);
#else
#ifdef MALLOC_INIT_ZERO
        bzero(ptr, size);
#endif
#endif
        return ptr;
    }

    void *calloc(size_t nmemb, size_t size) {
        uint32_t totsize = nmemb * size;
        size_t check = totsize / nmemb;
        if (check != size) {
            return nullptr;
        }
        return malloc(totsize);
    }

    uint32_t malloc_sizeof(void *ptr);

#ifdef DELAY_FREE_MEM
    hw_spinlock delaySpin;
    unsigned long int delayCountMem = 0;
    void *delayFreeMem[DELAY_FREE_MEM];
#endif

    void free(void *ptr) {
#ifdef FREE_MEM_POISON
        memset(ptr, 0xd0, malloc_sizeof(ptr));
#endif
#ifndef DISABLE_FREE_MEM
#ifdef DELAY_FREE_MEM
        void *freeptr = nullptr;
        {
            critical_section cli{};
            std::lock_guard lock{delaySpin};
            unsigned long int index = delayCountMem % DELAY_FREE_MEM;
            if (index < delayCountMem) {
                freeptr = delayFreeMem[index];
            }
            delayFreeMem[index] = ptr;
            ++delayCountMem;
        }
        if (freeptr != nullptr) {
            impl->free(freeptr);
        }
#else
        impl->free(ptr);
#endif
#endif
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
#ifdef DELAY_FREE_MEM
    hw_spinlock *cr = new (((void *) &delaySpin)) hw_spinlock();
    int delayCountMem = 0;
#endif
    wild_malloc_struct.malloc = wild_malloc;
    wild_malloc_struct.free = wild_free;
    wild_malloc_struct.sizeof_alloc = wild_sizeof_alloc;
    impl = &wild_malloc_struct;
    memoryAllocator = CreateChainedAllocatorRoot();
}

class wild_malloc_statistics : public statistics_object {
    void Accept(statistics_visitor &visitor) override;
};

void wild_malloc_statistics::Accept(statistics_visitor &visitor) {
    visitor.Visit("current_max", (long long int) wild_malloc_total());
    visitor.Visit("allocated", (long long int) wild_malloc_allocated());
}

class wild_malloc_statistics_factory : public statistics_object_factory {
public:
    std::shared_ptr<statistics_object> GetObject() override;
};

std::shared_ptr<statistics_object> wild_malloc_statistics_factory::GetObject() {
    return std::make_shared<wild_malloc_statistics>();
}

void setup_simplest_malloc_stats() {
    GetStatisticsRoot().Add("malloc", std::make_shared<wild_malloc_statistics_factory>());
}

void destroy_simplest_malloc_impl() {
    memoryAllocator->~MemoryAllocator();
}
