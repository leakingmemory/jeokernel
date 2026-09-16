//
// Created by sigsegv on 01.05.2021.
//

#include <core/vmem.h>
#include <pagealloc.h>

vmem::vmem(uint64_t size) {
    uint64_t overshoot = size & (pagesize() - 1);
    if (overshoot != 0) {
        size += pagesize() - overshoot;
    }
    this->base = vpagealloc(size);
    this->size = size;
}

void vmem::release() {
    if (this->base != 0) {
#if defined(__aarch64__)
        vpagefree(this->base, this->size);
#else
        vpagefree(this->base);
#endif
        this->base = 0;
    }
}

std::size_t vmem::npages() const {
    return this->size / pagesize();
}

vmem_page vmem::page(std::size_t pnum) {
    return vmem_page(((uint64_t) (pnum * pagesize())) + this->base);
}

void vmem::reload() {
    reload_pagetables();
}

void vmem_page::rmap(uint64_t paddr) {
    std::optional<pageentr> pe = get_pageentr(addr);
#if defined(__aarch64__)
    pe->value = 0;
    pe->pxn() = 1;
    pe->uxn() = 1;
    pe->ap() = 2;
    pe->attr_indx() = 1;
    pe->af() = 1;
    pe->sh() = 3;
    pe->table() = 1;
    pe->ppn() = paddr >> 12;
    pe->valid() = 1;
#else
    pe->dirty() = 0;
    pe->execution_disabled() = 1;
    pe->writeable() = 0;
    pe->accessed() = 0;
    pe->user_access() = 0;
    pe->write_through() = 0;
    pe->cache_disabled() = 0;
    pe->page_ppn() = paddr >> 12;
    pe->present() = 1;
#endif
    update_pageentr(addr, *pe);
}

void vmem_page::unmap() {
    std::optional<pageentr> pe = get_pageentr(addr);
#if defined(__aarch64__)
    pe->valid() = 0;
#else
    pe->present() = 0;
#endif
    update_pageentr(addr, *pe);
}

void vmem_page::rwmap(uint64_t paddr, bool write_through, bool cache_disabled) {
    std::optional<pageentr> pe = get_pageentr(addr);
#if defined(__aarch64__)
    pe->value = 0;
    pe->pxn() = 1;
    pe->uxn() = 1;
    pe->attr_indx() = 1;
    pe->af() = 1;
    pe->sh() = 3;
    pe->table() = 1;
    pe->ppn() = paddr >> 12;
    pe->valid() = 1;
#else
    pe->dirty() = 0;
    pe->execution_disabled() = 1;
    pe->writeable() = 1;
    pe->accessed() = 0;
    pe->user_access() = 0;
    pe->write_through() = write_through ? 1 : 0;
    pe->cache_disabled() = cache_disabled ? 1 : 0;
    pe->page_ppn() = paddr >> 12;
    pe->present() = 1;
#endif
    update_pageentr(addr, *pe);
}
