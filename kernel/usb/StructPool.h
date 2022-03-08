//
// Created by sigsegv on 9/11/21.
//

#ifndef JEOKERNEL_STRUCTPOOL_H
#define JEOKERNEL_STRUCTPOOL_H

#include <cstdint>
#include <optional>
#include <klogger.h>
#include <vector>
#include <mutex>
#include <concurrency/hw_spinlock.h>
#include <concurrency/critical_section.h>

template <typename Struct, typename PhysPtr> class StructPoolPointer {
private:
    Struct *pointer;
    PhysPtr phys;
public:
    Struct *Pointer() {
       return pointer;
    }
    PhysPtr Phys() {
        return phys;
    }
    StructPoolPointer(Struct *pointer, PhysPtr phys) : pointer(pointer), phys(phys) {
        new (pointer) Struct();
    }
    virtual ~StructPoolPointer() {
        pointer->~Struct();
    }
};

template <typename Allocator, typename Struct, typename PhysPtr> class StructPoolPointerImpl : public StructPoolPointer<Struct, PhysPtr> {
private:
    Allocator &allocator;
public:
    StructPoolPointerImpl(Allocator &allocator, Struct *pointer, PhysPtr phys) : StructPoolPointer<Struct, PhysPtr>(pointer, phys), allocator(allocator) {}
    virtual ~StructPoolPointerImpl() {
        allocator.Free(this->Phys());
    }
};

template <typename Pagealloc, typename Struct> class StructPoolAllocator {
public:
    typedef Struct type;
    typedef typename Pagealloc::physaddr_t PhysPtr;
private:
    Pagealloc page;
    uint32_t allocated;
    uint32_t map[((4096 / 32) / sizeof(type)) + 1];
    hw_spinlock &spinlock;
public:
    StructPoolAllocator(hw_spinlock &spinlock) : page(4096), allocated(0), map(), spinlock(spinlock) {
        for (int i = 0; i <= (ItemsPerPage() / 32); i++) {
            map[i] = 0;
        }
    }
    constexpr size_t ItemsPerPage() const {
        return 4096 / sizeof(type);
    }
    bool IsFree(size_t i) {
        std::lock_guard lock{spinlock};
        size_t index = i >> 5;
        uint32_t bit = (uint32_t) (i & 31);
        bit = 1 << bit;
        return (map[index] & bit) == 0;
    }
    void Free(PhysPtr phys) {
        std::lock_guard lock{spinlock};
        PhysPtr offset = phys - page.PhysAddr();
        PhysPtr i = offset / sizeof(type);
        size_t index = i >> 5;
        uint32_t bit = (uint32_t) (i & 31);
        bit = ~(1 << bit);
        map[index] = map[index] & bit;
    }
    std::optional<PhysPtr> Alloc() {
        if (allocated >= ItemsPerPage()) {
            return {};
        }
        for (int i = 0; i <= (ItemsPerPage() / 32); i++) {
            if (map[i] != 0xFFFFFFFF) {
                PhysPtr offset{0};
                uint32_t bit = 1;
                while ((map[i] & bit) != 0) {
                    ++offset;
                    bit = 1 << offset;
                }
                if (offset > 31) {
                    wild_panic("offset > 31");
                }
                offset += i << 5;
                if (offset >= ItemsPerPage()) {
                    return {};
                }
                ++allocated;
                map[i] = map[i] | bit;
                offset *= sizeof(Struct);
                return offset + page.PhysAddr();
            }
        }
        return {};
    }
    Struct *PointerTo(PhysPtr phys) {
        PhysPtr offset = phys - page.PhysAddr();
        uint8_t *ptr8 = (uint8_t *) page.Pointer();
        ptr8 += offset;
        return (Struct *) (void *) ptr8;
    }
};

template <typename Allocator> class StructPool {
public:
    typedef typename Allocator::type type;
    typedef typename Allocator::PhysPtr PhysPtr;
private:
    std::vector<Allocator *> allocators;
    hw_spinlock spinlock;
public:
    std::shared_ptr<StructPoolPointer<type, PhysPtr>> Alloc() {
        std::lock_guard lock{spinlock};
        for (Allocator *allocator : allocators) {
            std::optional<PhysPtr> phys = allocator->Alloc();
            if (phys) {
                auto *p = new StructPoolPointerImpl<Allocator, type, PhysPtr>(*allocator, allocator->PointerTo(*phys), *phys);
                return std::shared_ptr<StructPoolPointer<type, PhysPtr>>(p);
            }
        }
        auto *allocator = new Allocator(spinlock);
        std::optional<PhysPtr> phys = allocator->Alloc();
        allocators.push_back(allocator);
        auto *p = new StructPoolPointerImpl<Allocator, type, PhysPtr>(*allocator, allocator->PointerTo(*phys), *phys);
        return std::shared_ptr<StructPoolPointer<type, PhysPtr>>(p);
    }
};


#endif //JEOKERNEL_STRUCTPOOL_H
