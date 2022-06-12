//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/process.h>
#include <pagealloc.h>
#include <strings.h>
#include <mutex>
#include <thread>
#include "concurrency/raw_semaphore.h"

PagetableRoot::PagetableRoot(uint16_t addr) : physpage(ppagealloc(PAGESIZE)), vm(PAGESIZE), branches((pagetable *) vm.pointer()), addr(addr) {
    vm.page(0).rwmap(physpage);
    bzero(branches, sizeof(*branches));
}

PagetableRoot::~PagetableRoot() {
    constexpr uint32_t sizeOfUserspaceRoots = 512 - PMLT4_USERSPACE_START;
    if (physpage != 0) {
        {
            pagetable *table;
            vmem vm{sizeof(*table)};
            table = (pagetable *) vm.pointer();
            for (int i = 0; i < sizeOfUserspaceRoots; i++) {
                auto &b1 = (*branches)[i];
                if (b1.page_ppn == 0) {
                    continue;
                }
                vm.page(0).rwmap((b1.page_ppn << 12));
                vm.reload();
                for (int j = 0; j < 512; j++) {
                    auto &b2 = (*table)[j];
                    if (b2.page_ppn != 0) {
                        ppagefree((b2.page_ppn << 12), sizeof(*table));
                    }
                }
                ppagefree((b1.page_ppn << 12), sizeof(*table));
            }
        }
        vm.release();
        ppagefree(physpage, PAGESIZE);
        physpage = 0;
    }
}

PagetableRoot::PagetableRoot(PagetableRoot &&mv) : physpage(mv.physpage), vm(std::move(mv.vm)), branches(mv.branches), addr(mv.addr) {
}

MemMapping::MemMapping(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages)
: image(image), pagenum(pagenum), pages(pages), image_skip_pages(image_skip_pages) {}

Process::Process() : pagetableRoots(), mappings() {}

bool Process::Pageentry(uint32_t pagenum, std::function<void (pageentr &)> func) {
    constexpr uint32_t sizeOfUserspaceRoots = 512 - PMLT4_USERSPACE_START;
    uint32_t pageoff;
    uint32_t index;
    {
        constexpr uint32_t mask = 0x7FFFFFF;
        pageoff = pagenum & mask;
        static_assert((mask >> 26) == 1);
        index = pagenum >> 27;
    }
    if (pageoff >= sizeOfUserspaceRoots) {
        return false;
    }
    PagetableRoot *root = nullptr;
    for (auto &ptr : pagetableRoots) {
        if (ptr.addr == index) {
            root = &ptr;
            break;
        }
    }
    if (root == nullptr) {
        root = &(pagetableRoots.emplace_back(index));
    }
    {
        constexpr uint32_t mask = 0x3FFFF;
        pageoff = pageoff & mask;
        static_assert((mask >> 17) == 1);
        index = pageoff >> 18;
    }
    auto &b1 = (*(root->branches))[index];
    vmem vm{sizeof(pagetable)};
    static_assert(sizeof(pagetable) == vm.pagesize());
    pagetable *table = (pagetable *) vm.pointer();
    if (b1.page_ppn == 0) {
        auto phys = ppagealloc(sizeof(pagetable));
        if (phys == 0) {
            std::cerr << "Couldn't allocate phys page for pagetable\n";
            return false;
        }
        b1.present = 1;
        b1.writeable = 1;
        b1.user_access = 1;
        b1.page_ppn = (phys >> 12);
        b1.execution_disabled = 0;
        vm.page(0).rwmap((b1.page_ppn << 12));
        vm.reload();
        bzero(table, sizeof(*table));
    } else {
        vm.page(0).rwmap((b1.page_ppn << 12));
        vm.reload();
    }
    {
        constexpr uint32_t mask = 0x1FF;
        pageoff = pageoff & mask;
        static_assert((mask >> 8) == 1);
        index = pageoff >> 9;
    }
    auto &b2 = (*table)[index];
    if (b2.page_ppn == 0) {
        auto phys = ppagealloc(sizeof(pagetable));
        if (phys == 0) {
            std::cerr << "Couldn't allocate phys page for pagetable\n";
            return false;
        }
        b2.present = 1;
        b2.writeable = 1;
        b2.user_access = 1;
        b2.page_ppn = (phys >> 12);
        b2.execution_disabled = 0;
        vm.page(0).rwmap((b2.page_ppn << 12));
        vm.reload();
        bzero(table, sizeof(*table));
    } else {
        vm.page(0).rwmap((b2.page_ppn << 12));
        vm.reload();
    }
    auto &b3 = (*table)[pageoff];
    func(b3);
    return true;
}

bool Process::Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool write, bool execute, bool copyOnWrite) {
    std::cout << "Map " << std::hex << pagenum << "-" << (pagenum+pages) << " -> " << image_skip_pages << (write ? " write" : "") << (execute ? " exec" : "") << std::dec << "\n";
    if (write && !copyOnWrite) {
        std::cerr << "Error mapping: Write without COW is not supported\n";
    }
    for (const auto &mapping : mappings) {
        {
            auto overlap_page = mapping.pagenum > pagenum ? mapping.pagenum : pagenum;
            auto overlap_end = mapping.pagenum + mapping.pages;
            {
                auto other_end = pagenum + pages;
                if (other_end < overlap_end) {
                    overlap_end = other_end;
                }
            }
            if (overlap_page < overlap_end) {
                return false;
            }
        }
    }
    mappings.emplace_back(image, pagenum, pages, image_skip_pages);
    for (uint32_t p = 0; p < pages; p++) {
        bool success = Pageentry(pagenum + p, [execute, write] (pageentr &pe) {
            pe.present = 0;
            pe.writeable = 0;
            pe.user_access = 1;
            pe.write_through = 0;
            pe.cache_disabled = 0;
            pe.accessed = 0;
            pe.dirty = 0;
            pe.size = 0;
            pe.global = 0;
            pe.os_zero = 0;
            pe.os_virt_avail = 0;
            pe.page_ppn = 0;
            pe.reserved1 = 0;
            pe.os_virt_start = 0;
            pe.ignored3 = 0;
            pe.execution_disabled = !execute;
        });
        if (!success) {
            return false;
        }
    }
    return true;
}

void Process::task_enter() {
    critical_section cli{};
    auto &pt = get_root_pagetable();
    for (auto &root : pagetableRoots) {
        auto &pe = pt[root.addr + PMLT4_USERSPACE_START];
        pe.present = 1;
        pe.writeable = 1;
        pe.user_access = 1;
        pe.write_through = 0;
        pe.cache_disabled = 0;
        pe.accessed = 0;
        pe.dirty = 0;
        pe.size = 0;
        pe.global = 0;
        pe.os_zero = 0;
        pe.os_virt_avail = 0;
        pe.page_ppn = (root.physpage >> 12);
        pe.reserved1 = 0;
        pe.os_virt_start = 0;
        pe.ignored3 = 0;
        pe.execution_disabled = false;
    }
    reload_pagetables();
}

void Process::task_leave() {
}

struct process_pfault {
    task *task;
    Process *process;
    uintptr_t ip;
    uintptr_t address;
};

static hw_spinlock pfault_lck{};
static raw_semaphore pfault_sema{-1};
static std::vector<process_pfault> faults{};
static std::thread *pfault_thread{nullptr};

bool Process::page_fault(task &current_task, Interrupt &intr) {
    std::cout << "Page fault in user process\n";
    current_task.set_blocked(true);
    uint64_t address{0};
    asm("mov %%cr2, %0" : "=r"(address));
    std::lock_guard lock{pfault_lck};
    process_pfault pf{.task = &current_task, .process = this, .ip = intr.rip(), .address = address};
    faults.push_back(pf);
    pfault_sema.release();
    if (pfault_thread == nullptr) {
        pfault_thread = new std::thread([] () {
            std::this_thread::set_name("[pfault]");
            while (true) {
                pfault_sema.acquire();
                process_pfault pfault{};
                {
                    std::lock_guard lock{pfault_lck};
                    auto iterator = faults.begin();
                    if (iterator == faults.end()) {
                        continue;
                    }
                    pfault = *iterator;
                    faults.erase(iterator);
                }
                pfault.process->resolve_page_fault(*(pfault.task), pfault.ip, pfault.address);
            }
        });
    }
    return true;
}

void Process::resolve_page_fault(task &current_task, uintptr_t ip, uintptr_t fault_addr) {
    auto *scheduler = get_scheduler();
    std::cerr << "PID " << current_task.get_id() << ": Page fault at " << std::hex << ip << " addr " << fault_addr << std::dec << "\n";
    scheduler->terminate_blocked(&current_task);
}