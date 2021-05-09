//
// Created by sigsegv on 01.05.2021.
//

#include <core/vmem.h>
#include <pagealloc.h>

vmem::vmem(uint64_t size) {
    uint64_t overshoot = size & 0xFFF;
    if (overshoot != 0) {
        size += 0x1000 - overshoot;
    }
    this->base = vpagealloc(size);
    this->size = size;
}

void vmem::release() {
    if (this->base != 0) {
        vpagefree(this->base);
        this->base = 0;
    }
}

std::size_t vmem::npages() const {
    return this->size / 0x1000;
}

vmem_page vmem::page(std::size_t pnum) {
    return vmem_page(((uint64_t) (pnum * 0x1000)) + this->base);
}

void vmem::reload() {
    reload_pagetables();
}

void vmem_page::rmap(uint64_t paddr) {
    std::optional<pageentr> pe = get_pageentr(addr);
    pe->dirty = 0;
    pe->execution_disabled = 1;
    pe->writeable = 0;
    pe->accessed = 0;
    pe->user_access = 0;
    pe->page_ppn = paddr >> 12;
    pe->present = 1;
    update_pageentr(addr, *pe);
}

void vmem_page::unmap() {
    std::optional<pageentr> pe = get_pageentr(addr);
    pe->present = 0;
    update_pageentr(addr, *pe);
}

void vmem_page::rwmap(uint64_t paddr) {
    std::optional<pageentr> pe = get_pageentr(addr);
    pe->dirty = 0;
    pe->execution_disabled = 1;
    pe->writeable = 1;
    pe->accessed = 0;
    pe->user_access = 0;
    pe->page_ppn = paddr >> 12;
    pe->present = 1;
    update_pageentr(addr, *pe);
}
