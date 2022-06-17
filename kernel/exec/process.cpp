//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/process.h>
#include <pagealloc.h>
#include <strings.h>
#include <mutex>
#include <thread>
#include <kfs/kfiles.h>
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
: image(image), pagenum(pagenum), pages(pages), image_skip_pages(image_skip_pages), mappings() {}

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
        constexpr uint32_t mask = 0x7FFFFFF;
        pageoff = pageoff & mask;
        static_assert((mask >> 18) == 0x1FF);
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
        constexpr uint32_t mask = 0x3FFFF;
        pageoff = pageoff & mask;
        static_assert((mask >> 9) == 0x1FF);
        index = pageoff >> 9;
    }
    auto &b2 = (*table)[index];
    phys_t b2phys;
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
        b2phys = b2.page_ppn << 12;
        vm.page(0).rwmap(b2phys);
        vm.reload();
        bzero(table, sizeof(*table));
    } else {
        b2phys = b2.page_ppn << 12;
        vm.page(0).rwmap(b2phys);
        vm.reload();
    }
    auto &b3 = (*table)[pageoff & 0x1FF];
    func(b3);
    std::cout << std::hex << (b2phys + (pageoff & 0x1FF)) << " " << (b3.present != 0 ? "p" : "*") << (b3.user_access != 0 ? "u" : "k") << (b3.writeable != 0 ? "w" : "r")
              << (b3.execution_disabled != 0 ? "n" : "e") << " " << b3.page_ppn << std::dec
              << "\n";
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
        auto pe_index = root.addr + PMLT4_USERSPACE_START;
        auto &pe = pt[pe_index];
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
        pe.execution_disabled = 0;
    }
    reload_pagetables();
}

void Process::task_leave() {
}

struct process_pfault {
    task *pfaultTask;
    Process *process;
    uintptr_t ip;
    uintptr_t address;
};

static hw_spinlock pfault_lck{};
static raw_semaphore pfault_sema{-1};
static std::vector<process_pfault> faults{};
static std::thread *pfault_thread{nullptr};

bool Process::page_fault(task &current_task, Interrupt &intr) {
    std::cout << "Page fault in user process with pagetable root " << std::hex << ((uintptr_t) get_root_pagetable())
    << " code " << intr.error_code() << std::dec << "\n";
    current_task.set_blocked(true);
    uint64_t address{0};
    asm("mov %%cr2, %0" : "=r"(address));
    std::lock_guard lock{pfault_lck};
    process_pfault pf{.pfaultTask = &current_task, .process = this, .ip = intr.rip(), .address = address};
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
                pfault.process->resolve_page_fault(*(pfault.pfaultTask), pfault.ip, pfault.address);
            }
        });
    }
    return true;
}

bool Process::exception(task &current_task, const std::string &name, Interrupt &intr) {
    std::cout << "Exception <"<< name <<"> in user process at " << std::hex << intr.rip() << " code " << intr.error_code() << std::dec << "\n";
    current_task.set_end(true);
    return true;
}

void Process::resolve_page_fault(task &current_task, uintptr_t ip, uintptr_t fault_addr) {
    auto *scheduler = get_scheduler();
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    {
        constexpr uint64_t relocationOffset = ((uint64_t) PMLT4_USERSPACE_START) << (9 + 9 + 9 + 12);
        if (fault_addr < relocationOffset) {
            goto pfault_fail;
        }
        uint32_t page;
        {
            uintptr_t addr{fault_addr - relocationOffset};
            addr = addr >> 12;
            page = addr;
        }
        MemMapping *mapping{nullptr};
        for (auto &m : mappings) {
            if (m.pagenum <= page && page < (m.pagenum + m.pages)) {
                mapping = &m;
                break;
            }
        }
        if (mapping == nullptr) {
            goto pfault_fail;
        }
        phys_t physpage = mapping->image_skip_pages + (page - mapping->pagenum);
        uint16_t rootAddr;
        {
            uint32_t addr{page};
            addr = addr >> (9+9+9);
            rootAddr = addr;
        }
        PagetableRoot *root{nullptr};
        for (auto &r : pagetableRoots) {
            if (r.addr == rootAddr) {
                root = &r;
                break;
            }
        }
        if (root == nullptr) {
            goto pfault_fail;
        }
        auto b1 = (*(root->branches))[(page >> (9+9)) & 511];
        if (b1.present == 0) {
            goto pfault_fail;
        }
        vmem vm{PAGESIZE};
        vm.page(0).rmap(b1.page_ppn << 12);
        vm.reload();
        auto b2 = (*((pagetable *) vm.pointer()))[(page >> 9) & 511];
        if (b2.present == 0) {
            goto pfault_fail;
        }
        vm.page(0).rwmap(b2.page_ppn << 12);
        vm.reload();
        auto b3 = (*((pagetable *) vm.pointer()))[page & 511];
        if (b3.present == 0) {
            std::cout << "Loading page " << std::hex << page << " for addr " << fault_addr << " from " << physpage << std::dec << "\n";
            lock = {};
            auto image = mapping->image;
            auto loaded_page = image->GetPage(physpage);
            lock = std::make_unique<std::lock_guard<std::mutex>>(mtx);
            bool ok{false};
            for (auto &m : mappings) {
                if (m.pagenum <= page && page < (m.pagenum + m.pages)) {
                    if (mapping != &m || physpage != (m.image_skip_pages + (page - m.pagenum)) || image != m.image) {
                        goto pfault_fail;
                    }
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                goto pfault_fail;
            }
            ok = false;
            for (auto &r : pagetableRoots) {
                if (r.addr == rootAddr) {
                    if (root != &r) {
                        goto pfault_fail;
                    }
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                goto pfault_fail;
            }
            auto &b1 = (*(root->branches))[(page >> (9+9)) & 511];
            std::cout << "Root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(b1))) << std::dec << "\n";
            if (b1.present == 0) {
                goto pfault_fail;
            }
            vm.page(0).rmap(b1.page_ppn << 12);
            vm.reload();
            auto &b2 = (*((pagetable *) vm.pointer()))[(page >> 9) & 511];
            if (b2.present == 0) {
                goto pfault_fail;
            }
            auto b2phys = b2.page_ppn << 12;
            vm.page(0).rwmap(b2phys);
            vm.reload();
            auto &b3 = (*((pagetable *) vm.pointer()))[page & 511];
            if (b3.present == 0) {
                std::cout << std::hex << ((b2phys) + (page & 511)) << " *" << (b3.user_access != 0 ? "u" : "k") << (b3.writeable != 0 ? "w" : "r")
                << (b3.execution_disabled != 0 ? "n" : "e") << " " << b3.page_ppn << std::dec
                << "\n";
                mapping->mappings.push_back({.data = loaded_page, .page = page});
                b3.page_ppn = loaded_page.PhysAddr() >> 12;
                b3.present = 1;
                reload_pagetables();
            }
            current_task.set_blocked(false);
            return;
        } else {
        }
    }
pfault_fail:
    std::cerr << "PID " << current_task.get_id() << ": Page fault at " << std::hex << ip << " addr " << fault_addr << std::dec << "\n";
    scheduler->terminate_blocked(&current_task);
}