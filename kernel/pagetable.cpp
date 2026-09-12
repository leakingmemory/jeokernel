//
// Created by sigsegv on 20.04.2021.
//

#include <stdint.h>
#include <pagetable_impl.h>
#include <concurrency/hw_spinlock.h>
#include <new>
#include <mutex>
#include <concurrency/critical_section.h>

static uint8_t __pagetables_lock_mem[sizeof(hw_spinlock)];
static hw_spinlock *pagetables_lock;

void initialize_pagetable_control() {
    pagetables_lock = new ((void *) &(__pagetables_lock_mem[0])) hw_spinlock();
}

hw_spinlock &get_pagetables_lock() {
    return *pagetables_lock;
}

extern uintptr_t init_pml4t_addr;

#define _get_pml4t()  (*((pagetable *) ((uintptr_t) get_pagetable_virt_offset() + init_pml4t_addr)))

#if !defined(__aarch64__)
static uintptr_t pagetable_virt_offset = 0;

uintptr_t get_pagetable_virt_offset() {
    return pagetable_virt_offset;
}

void set_pagetable_virt_offset(uintptr_t offset) {
    pagetable_virt_offset = offset;
}

#else

static PageentrAvailablePages pageentr_available_pages;

void init_mapping_pages(uint64_t vaddr) {
    for (uint64_t i = 0; i < 8; i++) {
        pageentr_available_pages.Fill(vaddr + (i << 12));
    }
}

#endif

std::optional<pageentr> get_pageentr(uint64_t addr) {
    critical_section cli{};
    std::lock_guard lock{*pagetables_lock};

#if defined(__aarch64__)
    pageentr *pe = get_pageentr64(_get_pml4t(), addr, pageentr_available_pages);
#else
    pageentr *pe = get_pageentr64(_get_pml4t(), addr);
#endif
    if (pe != nullptr) {
        return *pe;
    } else {
        return {};
    }
}

bool update_pageentr(uint64_t addr, const pageentr &pe_vmem_update) {
    critical_section cli{};
    std::lock_guard lock{*pagetables_lock};

#if defined(__aarch64__)
    pageentr *pe = get_pageentr64(_get_pml4t(), addr, pageentr_available_pages);
#else
    pageentr *pe = get_pageentr64(_get_pml4t(), addr);
#endif
    if (pe != nullptr) {
#if defined(__aarch64__)
        pe->valid() = pe_vmem_update.valid();
        pe->table() = pe_vmem_update.table();
        pe->attr_indx() = pe_vmem_update.attr_indx();
        pe->ns() = pe_vmem_update.ns();
        pe->ap() = pe_vmem_update.ap();
        pe->pxn() = pe_vmem_update.pxn();
        pe->uxn() = pe_vmem_update.uxn();
        pe->sh() = pe_vmem_update.sh();
        pe->af() = pe_vmem_update.af();
        pe->dbm() = pe_vmem_update.dbm();
        pe->ppn() = pe_vmem_update.ppn();
        pe->contiguous() = pe_vmem_update.contiguous();
#else
        pe->present() = pe_vmem_update.present();
        pe->writeable() = pe_vmem_update.writeable();
        pe->execution_disabled() = pe_vmem_update.execution_disabled();
        pe->cache_disabled() = pe_vmem_update.cache_disabled();
        pe->write_through() = pe_vmem_update.write_through();
        pe->user_access() = pe_vmem_update.user_access();
        pe->accessed() = pe_vmem_update.accessed();
        pe->dirty() = pe_vmem_update.dirty();
        pe->page_ppn() = pe_vmem_update.page_ppn();
#endif
        return true;
    } else {
        return false;
    }
}

#if defined(__x86_64__)
bool update_pageentr(uint64_t addr, std::function<void (pageentr &pe)> func) {
    critical_section cli{};
    std::lock_guard lock{*pagetables_lock};

    pageentr *pe = get_pageentr64(_get_pml4t(), addr);
    if (pe != nullptr) {
        func(*pe);
        return true;
    } else {
        return false;
    }
}
#endif

uint64_t get_phys_from_virt(uint64_t vaddr) {
    std::optional<pageentr> pe = get_pageentr(vaddr);
    if (pe) {
        uint64_t offset = vaddr & 0x0FFF;
#if defined(__aarch64__)
        uint64_t paddr = (*pe).ppn();
#else
        uint64_t paddr = (*pe).page_ppn();
#endif
        paddr = paddr << 12;
        return paddr + offset;
    } else {
        return 0;
    }
}
