//
// Created by sigsegv on 6/5/22.
//

#include <iostream>
#include <files/filepage.h>
#include <kfs/filepage_data.h>
#include <pagealloc.h>
#include <klogger.h>
#include <core/vmem.h>
#include <strings.h>

#define FP_PAGESIZE FILEPAGE_PAGE_SIZE

class filepage_pointer_impl : public filepage_pointer {
private:
    vmem vm;
public:
    filepage_pointer_impl(uintptr_t addr) : vm(FP_PAGESIZE) {
        vm.page(0).rwmap(addr);
    }
    void *Pointer() const override;
    void SetDirty(size_t length) override;
};

void *filepage_pointer_impl::Pointer() const {
    return vm.pointer();
}

void filepage_pointer_impl::SetDirty(size_t length) {
}

filepage::filepage() : page(new filepage_data) {}

std::shared_ptr<filepage_pointer> filepage::Pointer() {
    if (page->physpage != 0) {
        return std::make_shared<filepage_pointer_impl>(page->physpage);
    } else {
        return {};
    }
}

std::shared_ptr<filepage_data> filepage::Raw() {
    return page;
}

void filepage::Zero() {
    bzero(Pointer()->Pointer(), FP_PAGESIZE);
}

void filepage::SetDirty(size_t length) {
    page->setDirty(length);
}

bool filepage::IsDirty() const {
    return page->getDirty() > 0;
}

size_t filepage::GetDirtyLength() const {
    return page->getDirty();
}

size_t filepage::GetDirtyLengthAndClear() {
    return page->getDirtyAndClear();
}

filepage_data::filepage_data() : physpage(0), ref(1), initRef(1), dirty(0) {
    physpage = ppagealloc(FP_PAGESIZE);
    if (physpage == 0) {
        wild_panic("Out of phys pages (filepage_data)");
    }
}

filepage_data::~filepage_data() {
    if (physpage != 0) {
        ppagefree(physpage, FP_PAGESIZE);
        physpage = 0;
    }
}

void filepage_data::up() {
    asm("lock incl %0" : "=m"(ref));
}

void filepage_data::down() {
    asm("lock decl %0" : "=m"(ref));
}

void filepage_data::initDone() {
    uint32_t ref;
    asm("xor %%rax, %%rax; dec %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(initRef), "=rm"(ref) :: "%rax");
    if (ref == 1) {
        down();
    }
}

void filepage_data::setDirty(uint32_t dirty) {
    uint64_t set{dirty};
    uint64_t expectdirty{0};
    uint64_t olddirty{0};
    do {
        uint64_t expect{expectdirty};
        expectdirty = olddirty;
        if (expect > set) {
            break;
        }
        asm(""
            "mov %1, %%rax;" // set
            "mov %2, %%rcx;" // expect
            "lock cmpxchgl %%ecx, %0;"
            "mov %%rcx, %3"
                : "+m"(this->dirty), "=r"(set), "=r"(expect), "=rm"(olddirty) :: "%rax", "%rcx");
    } while (olddirty != expectdirty);
}

uint32_t filepage_data::getDirty() {
    asm("mfence");
    return dirty;
}

uint32_t filepage_data::getDirtyAndClear() {
    asm("mfence");
    uint64_t beforeClear;
    uint64_t atClear{dirty};
    do {
        beforeClear = atClear;
        asm(""
            "xor %%rax, %%rax;" // set
            "mov %1, %%rcx;" // expect
            "lock cmpxchgl %%ecx, %0;"
            "mov %%rcx, %2"
                : "+m"(this->dirty), "=r"(beforeClear), "=rm"(atClear) :: "%rax", "%rcx");
    } while (atClear != beforeClear);
    return beforeClear;
}
