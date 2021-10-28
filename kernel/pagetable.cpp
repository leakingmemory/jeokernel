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

#define _get_pml4t()  (*((pagetable *) 0x1000))

std::optional<pageentr> get_pageentr(uint64_t addr) {
    critical_section cli{};
    std::lock_guard lock{*pagetables_lock};

    pageentr *pe = get_pageentr64(_get_pml4t(), addr);
    if (pe != nullptr) {
        pageentr cp = *pe;
        std::optional<pageentr> opt{cp};
        return opt;
    } else {
        std::optional<pageentr> opt{};
        return opt;
    }
}

bool update_pageentr(uint64_t addr, const pageentr &pe_vmem_update) {
    critical_section cli{};
    std::lock_guard lock{*pagetables_lock};

    pageentr *pe = get_pageentr64(_get_pml4t(), addr);
    if (pe != nullptr) {
        pe->present = pe_vmem_update.present;
        pe->writeable = pe_vmem_update.writeable;
        pe->execution_disabled = pe_vmem_update.execution_disabled;
        pe->cache_disabled = pe_vmem_update.cache_disabled;
        pe->write_through = pe_vmem_update.write_through;
        pe->user_access = pe_vmem_update.user_access;
        pe->accessed = pe_vmem_update.accessed;
        pe->dirty = pe_vmem_update.dirty;
        pe->page_ppn = pe_vmem_update.page_ppn;
        return true;
    } else {
        return false;
    }
}

uint64_t get_phys_from_virt(uint64_t vaddr) {
    std::optional<pageentr> pe = get_pageentr(vaddr);
    if (pe) {
        uint64_t offset = vaddr & 0x0FFF;
        uint64_t paddr = (*pe).page_ppn;
        paddr = paddr << 12;
        return paddr + offset;
    } else {
        return 0;
    }
}
