//
// Created by sigsegv on 19.04.2021.
//

#ifndef JEOKERNEL_MALLOCATOR_H
#define JEOKERNEL_MALLOCATOR_H

#ifndef UNIT_TESTING

#include <pagealloc.h>

#endif

typedef uint8_t small_alloc_unit[16];

#define ALLOCATED_MASK 0x55

struct SmallUnitMemoryAllocTable {
    uint8_t bits[4096];

    small_alloc_unit *begin_ptr() {
        uint8_t *ptr = (uint8_t *) this;
        ptr += 4096;
        return (small_alloc_unit *) ptr;
    }
    small_alloc_unit *end_ptr() {
        small_alloc_unit *ptr = begin_ptr();
        return &(ptr[4096 * 4]);
    }
    uint64_t max_allocation_size() {
        return sizeof(small_alloc_unit) * 4096 * 4;
    }
    small_alloc_unit *pointer_to(uint64_t position) {
        return &(begin_ptr()[position]);
    }
    uint8_t get_bits(uint64_t position) {
        uint64_t bytepos = position >> 2;
        uint8_t bitpos = (((uint8_t) (position & 3)) << 1);
        uint8_t bitmask = 3 << bitpos;
        return (bits[bytepos] & bitmask) >> bitpos;
    }
    bool is_free(uint64_t position) {
        uint64_t bytepos = position >> 2;
        uint8_t bitmask = 1 << (((uint8_t) (position & 3)) << 1);
        return (bits[bytepos] & bitmask) == 0;
    }
    void set_allocation(uint64_t position, uint64_t ibits) {
        uint64_t bytepos = position >> 2;
        uint8_t bitmask = ~(3 << (((uint8_t) (position & 3)) << 1));
        ibits = ibits << (((uint8_t) (position & 3)) << 1);
        bits[bytepos] &= bitmask;
        bits[bytepos] |= ibits;
    }

    void *sm_allocate(uint32_t size) {
        if (size > max_allocation_size()) {
            return nullptr;
        }
        if (size < sizeof(small_alloc_unit)) {
            size = sizeof(small_alloc_unit);
        } else {
            uint32_t overshoot = size % sizeof(small_alloc_unit);
            if (overshoot != 0) {
                size += sizeof(small_alloc_unit) - overshoot;
            }
        }
        size /= sizeof(small_alloc_unit);
        uint32_t count = 0;
        uint32_t i = 0;
        while (i < 4096) {
            uint8_t masked = bits[i] & ALLOCATED_MASK;
            if (masked != ALLOCATED_MASK) {
                for (int j = 0; j < 4; j++) {
                    uint32_t pos = i;
                    pos = pos << 2;
                    pos += j;
                    if (is_free(pos)) {
                        ++count;
                        if (count == size) {
                            pos -= count - 1;
                            set_allocation(pos, 3);
                            for (uint32_t i = 1; i < count; i++) {
                                set_allocation(pos + i, 1);
                            }
                            return pointer_to(pos);
                        }
                    } else {
                        count = 0;
                    }
                }
            } else {
                count = 0;
            }
            ++i;
        }
        return nullptr;
    }
    bool sm_owned(void *ptr) {
        if (ptr >= begin_ptr() && ptr < end_ptr()) {
            return true;
        } else {
            return false;
        }
    }
    uint32_t sm_free(void *ptr) {
        uint64_t vector = ((uint64_t) ptr) - ((uint64_t) begin_ptr());
        if (vector % sizeof(small_alloc_unit)) {
            return 0;
        }
        vector = vector / sizeof(small_alloc_unit);
        set_allocation(vector, 0);
        ++vector;
        uint32_t size = sizeof(small_alloc_unit);
        while (vector < (4 * 4096)) {
            uint8_t bits = get_bits(vector);
            if (bits == 0 || bits == 3) {
                return size;
            }
            set_allocation(vector, 0);
            ++vector;
            size += sizeof(small_alloc_unit);
        }
        return size;
    }

    uint32_t sm_sizeof(void *ptr) {
        uint64_t vector = ((uint64_t) ptr) - ((uint64_t) begin_ptr());
        if (vector % sizeof(small_alloc_unit)) {
            return 0;
        }
        uint32_t size;
        vector = vector / sizeof(small_alloc_unit);
        ++vector;
        while (vector < (4 * 4096)) {
            uint8_t bits = get_bits(vector);
            if (bits == 0 || bits == 3) {
                return size * sizeof(small_alloc_unit);
            }
            ++vector;
        }
        return size * sizeof(small_alloc_unit);
    }
} __attribute__((__packed__));

static_assert(sizeof(SmallUnitMemoryAllocTable) == 4096);

struct SmallUnitMemoryArea {
    SmallUnitMemoryAllocTable alloctable;
    //small_alloc_unit units[4096 * 4];
};

class MemoryAllocator {
public:
    virtual ~MemoryAllocator() {
    }
    virtual void *sm_allocate(uint32_t size) = 0;
    virtual uint32_t sm_free(void *ptr) = 0;
    virtual uint32_t sm_sizeof(void *ptr) = 0;
    virtual bool sm_owned(void *ptr) = 0;
};

/*
 * Allocate virtual memory space for it and physical the first two pages
 */
struct BasicMemoryAllocatorImpl {
    SmallUnitMemoryArea memoryArea; /* 4K / 1 page */
    /* Do not add data fields */

    void *sm_allocate(uint32_t size);
    uint32_t sm_free(void *ptr);
    uint32_t sm_sizeof(void *ptr);
    bool sm_owned(void *ptr);

    BasicMemoryAllocatorImpl() {
        memoryArea.alloctable = {};
    }
    ~BasicMemoryAllocatorImpl();
};

static_assert(sizeof(BasicMemoryAllocatorImpl) == 4096);

class BasicMemoryAllocator : public MemoryAllocator {
public:
    hw_spinlock _lock;
    BasicMemoryAllocatorImpl *impl;
    uint32_t consumed;

    void Consume(uint32_t size);
    void Release(uint32_t size);
    void *sm_allocate(uint32_t size) override;
    uint32_t sm_free(void *ptr) override;
    uint32_t sm_sizeof(void *ptr) override;
    bool sm_owned(void *ptr) override;
    explicit BasicMemoryAllocator(BasicMemoryAllocatorImpl *impl);
    ~BasicMemoryAllocator() override;
};

BasicMemoryAllocator *CreateBasicMemoryAllocator();

#endif //JEOKERNEL_MALLOCATOR_H
