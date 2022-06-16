//
// Created by sigsegv on 23.04.2021.
//

#include "tests.h"
#include <stdint.h>
#include <memory>
typedef uintptr_t phys_t;
#include "../include/concurrency/hw_spinlock.h"
#include "../include/pagetable.h"
#include "../include/pagealloc.h"
#include "../kernel/kmemory/mallocator.h"
#include <string>

std::string hex8(uint32_t num) {
    char str[3];
    str[0] = num >> 4;
    str[1] = num & 15;
    str[0] += str[0] < 10 ? '0' : 'A'-10;
    str[1] += str[1] < 10 ? '0' : 'A'-10;
    str[2] = 0;
    return std::string{str};
}

std::string hex16(uint16_t num) {
    std::string str{hex8((uint8_t) (num >> 8))};
    str.append(hex8((uint8_t) num));
    return str;
}

std::string hex32(uint32_t num) {
    std::string str{hex16((uint16_t) (num >> 16))};
    str.append(hex16((uint16_t) num));
    return str;
}

std::string hex64(uint64_t num) {
    std::string str{hex32((uint32_t) (num >> 32))};
    str.append(hex32((uint32_t) num));
    return str;
}

int main() {
    SmallUnitMemoryArea memoryArea{};
    //static_assert(sizeof(SmallUnitMemoryArea) == 0x41000);
    std::cout << "Memory area " << hex64((uint64_t) &memoryArea) << "-" << hex64(((uint64_t) &memoryArea) + sizeof(memoryArea) - 1) << std::endl;
    uint64_t size = 16;
    void *ptrs[15];
    for (int i = 0; i < 14; i++) {
        void *ptr = memoryArea.alloctable.sm_allocate(size);
        assert(ptr != nullptr);
        std::cout << "Alloc " << hex64((uint64_t) ptr) << "-" << hex64(((uint64_t) ptr) + size - 1) << std::endl;
        ptrs[i] = ptr;
        size = size << 1;
    }
    assert(memoryArea.alloctable.sm_allocate(size) == nullptr);
    assert(memoryArea.alloctable.sm_allocate(17) == nullptr);
    {
        void *ptr = memoryArea.alloctable.sm_allocate(16);
        std::cout << "Alloc " << hex64((uint64_t) ptr) << "-" << hex64(((uint64_t) ptr) + 15) << std::endl;
        ptrs[14] = ptr;
    }
    assert(ptrs[14] != nullptr);
    assert(memoryArea.alloctable.sm_allocate(1) == nullptr);
    size = 16;
    for (int i = 0; i < 14; i++) {
        std::cout << "Free " << hex64((uint64_t) ptrs[i]) << std::endl;
        memoryArea.alloctable.sm_free(ptrs[i]);
        assert(memoryArea.alloctable.sm_allocate(size + 1) == nullptr);
        ptrs[i] = memoryArea.alloctable.sm_allocate(size);
        assert(ptrs[i] != nullptr);
        assert(memoryArea.alloctable.sm_allocate(size) == nullptr);
        size = size << 1;
    }
    assert(memoryArea.alloctable.sm_allocate(1) == nullptr);
    memoryArea.alloctable.sm_free(ptrs[14]);
    assert(memoryArea.alloctable.sm_allocate(17) == nullptr);
    ptrs[14] = memoryArea.alloctable.sm_allocate(16);
    assert(ptrs[12] != nullptr);
    assert(memoryArea.alloctable.sm_allocate(1) == nullptr);
    for (int i = 0; i < 14; i++) {
        memoryArea.alloctable.sm_free(ptrs[i]);
    }
    ptrs[0] = memoryArea.alloctable.sm_allocate(0x40000 - 16);
    assert(ptrs[0] != nullptr);
    assert(memoryArea.alloctable.sm_allocate(1) == nullptr);
    memoryArea.alloctable.sm_free(ptrs[0]);
    return 0;
}