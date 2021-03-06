//
// Created by sigsegv on 6/8/22.
//

#include <sys/types.h>
#include <iostream>
#include <exec/process.h>
#include <pagealloc.h>
#include <strings.h>
#include <mutex>
#include <thread>
#include <kfs/kfiles.h>
#include "concurrency/raw_semaphore.h"
#include "StdinDesc.h"
#include "StdoutDesc.h"

//#define DEBUG_PROCESS_PAGEENTR

static hw_spinlock pidLock{};
static std::vector<pid_t> pids{};
static pid_t next = 1;

PagetableRoot::PagetableRoot(uint16_t addr) : physpage(ppagealloc(PAGESIZE)), vm(PAGESIZE), branches((pagetable *) vm.pointer()), addr(addr) {
    vm.page(0).rwmap(physpage);
    bzero(branches, sizeof(*branches));
}

PagetableRoot::~PagetableRoot() {
    constexpr uint32_t sizeOfUserspaceRoots = 512 - PMLT4_USERSPACE_HIGH_START;
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

MemMapping::MemMapping(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool cow, bool binaryMapping)
: image(image), pagenum(pagenum), pages(pages), image_skip_pages(image_skip_pages), cow(cow), binary_mapping(binaryMapping), mappings() {}

MemMapping::~MemMapping() {
    for (const auto &ppage : mappings) {
        if (!ppage.data) {
            phys_t addr = ppage.phys_page;
            ppagefree(addr, PAGESIZE);
        }
    }
}

static pid_t FindNextPid() {
    auto numPids = pids.size();
    uint32_t i = 0;
    while (i < numPids) {
        if (next < pids[i]) {
            pid_t pid = next;
            ++next;
            return pid;
        }
        if (next == pids[i]) {
            ++next;
            if (next < 1) {
                next = 1;
                return FindNextPid();
            }
        }
        ++i;
    }
    pid_t pid = next;
    ++next;
    return pid;
}

static pid_t AllocPid() {
    std::lock_guard lock{pidLock};
    pid_t pid = FindNextPid();
    auto numPids = pids.size();
    pids.push_back(0);
    uint32_t i = 0;
    while (i < numPids) {
        if (pid < pids[i]) {
            uint32_t j = numPids;
            while (i < j) {
                --j;
                pids[j + 1] = pids[j];
            }
            pids[i] = pid;
            return pid;
        }
        ++i;
    }
    pids[numPids] = pid;
    return pid;
}

Process::Process() : pid(0), pagetableLow(), pagetableRoots(), mappings(), fileDescriptors(), euid(0), egid(0), uid(0), gid(0) {
    fileDescriptors.push_back(StdinDesc::Descriptor());
    fileDescriptors.push_back(StdoutDesc::StdoutDescriptor());
    fileDescriptors.push_back(StdoutDesc::StderrDescriptor());
    pid = AllocPid();
}

std::optional<pageentr> Process::Pageentry(uint32_t pagenum) {
    if ((pagenum >> (9+9+9)) < PMLT4_USERSPACE_HIGH_START) {
        uint32_t lowRoot = pagenum >> (9+9);
        if (lowRoot >= USERSPACE_LOW_END) {
            return {};
        }
        PagetableRoot *root = nullptr;
        for (auto &ptr: pagetableLow) {
            if (ptr.addr == lowRoot) {
                root = &ptr;
                break;
            }
        }
        if (root == nullptr) {
            return {};
        }
        uint32_t pageoff;
        uint32_t index;
        {
            constexpr uint32_t mask = 0x3FFFF;
            pageoff = pagenum & mask;
            static_assert((mask >> 9) == 0x1FF);
            index = pageoff >> 9;
        }
        const auto &b1 = (*(root->branches))[index];
        vmem vm{sizeof(pagetable)};
        static_assert(sizeof(pagetable) == vm.pagesize());
        pagetable *table = (pagetable *) vm.pointer();
        if (b1.page_ppn == 0) {
            return {};
        }
        vm.page(0).rwmap((b1.page_ppn << 12));
        vm.reload();
        const auto &b2 = (*table)[pageoff & 0x1FF];
        return b2;
    } else {
        constexpr uint32_t sizeOfUserspaceRoots = PMLT4_USERSPACE_HIGH_END - PMLT4_USERSPACE_HIGH_START;
        uint32_t pageoff;
        uint32_t index;
        {
            constexpr uint32_t mask = 0x7FFFFFF;
            pageoff = pagenum & mask;
            static_assert((mask >> 26) == 1);
            index = pagenum >> 27;
        }
        if (index >= sizeOfUserspaceRoots) {
            return {};
        }
        PagetableRoot *root = nullptr;
        for (auto &ptr: pagetableRoots) {
            if (ptr.addr == index) {
                root = &ptr;
                break;
            }
        }
        if (root == nullptr) {
            return {};
        }
        {
            constexpr uint32_t mask = 0x7FFFFFF;
            pageoff = pageoff & mask;
            static_assert((mask >> 18) == 0x1FF);
            index = pageoff >> 18;
        }
        const auto &b1 = (*(root->branches))[index];
        vmem vm{sizeof(pagetable)};
        static_assert(sizeof(pagetable) == vm.pagesize());
        pagetable *table = (pagetable *) vm.pointer();
        if (b1.page_ppn == 0) {
            return {};
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
        const auto &b2 = (*table)[index];
        phys_t b2phys;
        if (b2.page_ppn == 0) {
            return {};
        } else {
            b2phys = b2.page_ppn << 12;
            vm.page(0).rwmap(b2phys);
            vm.reload();
        }
        const auto &b3 = (*table)[pageoff & 0x1FF];
        return b3;
    }
}

bool Process::Pageentry(uint32_t pagenum, std::function<void (pageentr &)> func) {
    if ((pagenum >> (9+9+9)) < PMLT4_USERSPACE_HIGH_START) {
        uint32_t lowRoot = pagenum >> (9+9);
        if (lowRoot >= USERSPACE_LOW_END) {
            return false;
        }
        PagetableRoot *root = nullptr;
        for (auto &ptr: pagetableLow) {
            if (ptr.addr == lowRoot) {
                root = &ptr;
                break;
            }
        }
        if (root == nullptr) {
            root = &(pagetableLow.emplace_back(lowRoot));
        }
        uint32_t pageoff;
        uint32_t index;
        {
            constexpr uint32_t mask = 0x3FFFF;
            pageoff = pagenum & mask;
            static_assert((mask >> 9) == 0x1FF);
            index = pageoff >> 9;
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
        auto &b2 = (*table)[pageoff & 0x1FF];
        func(b2);
#ifdef DEBUG_PROCESS_PAGEENTR
        std::cout << std::hex << " " << ((uintptr_t) &b1)<<"=>"<<((uintptr_t) &b2) << " "
                  << (b2.present != 0 ? "p" : "*") << (b2.user_access != 0 ? "u" : "k")
                  << (b2.writeable != 0 ? "w" : "r") << (b2.execution_disabled != 0 ? "n" : "e") << " " << b2.page_ppn
                  << std::dec << "\n";
#endif
        return true;
    } else {
        constexpr uint32_t sizeOfUserspaceRoots = PMLT4_USERSPACE_HIGH_END - PMLT4_USERSPACE_HIGH_START;
        uint32_t pageoff;
        uint32_t index;
        {
            constexpr uint32_t mask = 0x7FFFFFF;
            pageoff = pagenum & mask;
            static_assert((mask >> 26) == 1);
            index = pagenum >> 27;
        }
        if (index >= (sizeOfUserspaceRoots + PMLT4_USERSPACE_HIGH_START)) {
            return false;
        }
        PagetableRoot *root = nullptr;
        for (auto &ptr: pagetableRoots) {
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
#ifdef DEBUG_PROCESS_PAGEENTR
        std::cout << std::hex << (b2phys + (pageoff & 0x1FF)) << " " << (b3.present != 0 ? "p" : "*")
                  << (b3.user_access != 0 ? "u" : "k") << (b3.writeable != 0 ? "w" : "r")
                  << (b3.execution_disabled != 0 ? "n" : "e") << " " << b3.page_ppn << std::dec
                  << "\n";
#endif
        return true;
    }
}

bool Process::CheckMapOverlap(uint32_t pagenum, uint32_t pages) {
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
    return true;
}

struct MemoryArea {
    uint32_t start;
    uint32_t end;

    uint32_t Length() {
        return end - start;
    }
    MemoryArea Combine(const MemoryArea &other) {
        MemoryArea area{other};
        if (area.start > start) {
            area.start = start;
        }
        if (area.end < end) {
            area.end = end;
        }
        return area;
    }
    bool Overlaps(MemoryArea &other) {
        return (Combine(other).Length() < (Length() + other.Length()));
    }
};

uint32_t Process::FindFree(uint32_t pages) {
    std::vector<MemoryArea> freemem{};
    {
        std::vector<MemoryArea> mappings{};
        {
            mappings.reserve(this->mappings.size());
            for (int i = 0; i < this->mappings.size(); i++) {
                auto &mapping = this->mappings[i];
                MemoryArea area{.start = mapping.pagenum, .end = (mapping.pagenum + mapping.pages)};
                auto iterator = mappings.begin();
                while (iterator != mappings.end()) {
                    if (iterator->Overlaps(area)) {
                        area = iterator->Combine(area);
                        mappings.erase(iterator);
                        continue;
                    }
                    ++iterator;
                }
                auto num = mappings.size();
                mappings.push_back({});
                bool between = false;
                for (int i = 0; i < num; i++) {
                    if (mappings[i].start > area.start) {
                        between = true;
                        for (int j = num; j > i; j--) {
                            mappings[j] = mappings[j - 1];
                        }
                        mappings[i] = area;
                        break;
                    }
                }
                if (!between) {
                    mappings[num] = area;
                }
            }
        }

        constexpr uint32_t lowLim = ((uintptr_t) PMLT4_USERSPACE_HIGH_START) << (9 + 9 + 9);
        constexpr uintptr_t highLimBase = ((uintptr_t) PMLT4_USERSPACE_HIGH_END) << (9 + 9 + 9);
        constexpr uint32_t highLim = highLimBase < 0x100000000 ? highLimBase : 0xFFFFFFFF;

        auto start = lowLim;
        auto iterator = mappings.begin();
        while (iterator != mappings.end()) {
            if (iterator->start > start) {
                freemem.push_back({.start = start, .end = iterator->start});
            }
            if (iterator->end > start) {
                start = iterator->end;
            }
            ++iterator;
        }
        if (start < highLim) {
            freemem.push_back({.start = start, .end = highLim});
        }
    }
    MemoryArea *memoryArea = nullptr;
    for (auto &fmem : freemem) {
        auto size = fmem.end - fmem.start;
        if (size >= pages) {
            if (memoryArea == nullptr) {
                memoryArea = &fmem;
            } else {
                auto compSize = memoryArea->end - memoryArea->start;
                if (compSize > size) {
                    memoryArea = &fmem;
                }
            }
        }
    }
    if (memoryArea == nullptr) {
        return 0;
    }
    return memoryArea->end - pages;
}

bool Process::Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool write, bool execute, bool copyOnWrite, bool binaryMap) {
    std::cout << "Map " << std::hex << pagenum << "-" << (pagenum+pages) << " -> " << image_skip_pages << (write ? " write" : "") << (execute ? " exec" : "") << std::dec << "\n";
    if (write && !copyOnWrite) {
        std::cerr << "Error mapping: Write without COW is not supported\n";
    }
    if (!CheckMapOverlap(pagenum, pages)) {
        return false;
    }
    mappings.emplace_back(image, pagenum, pages, image_skip_pages, write && copyOnWrite, binaryMap);
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
            pe.ignored2 = 0;
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

bool Process::Map(uint32_t pagenum, uint32_t pages, bool binaryMap) {
    std::cout << "Map " << std::hex << pagenum << "-" << (pagenum+pages) << " on demand " << std::dec << "\n";
    if (!CheckMapOverlap(pagenum, pages)) {
        return false;
    }
    mappings.emplace_back(std::shared_ptr<kfile>(), pagenum, pages, 0, false, binaryMap);
    for (uint32_t p = 0; p < pages; p++) {
        bool success = Pageentry(pagenum + p, [] (pageentr &pe) {
            pe.present = 0;
            pe.writeable = 1;
            pe.user_access = 1;
            pe.write_through = 0;
            pe.cache_disabled = 0;
            pe.accessed = 0;
            pe.dirty = 0;
            pe.size = 0;
            pe.global = 0;
            pe.ignored2 = 0;
            pe.os_virt_avail = 0;
            pe.page_ppn = 0;
            pe.reserved1 = 0;
            pe.os_virt_start = 0;
            pe.ignored3 = 0;
            pe.execution_disabled = 0;
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
    if constexpr(USERSPACE_LOW_END > 0) {
        for (auto &root : pagetableLow) {
            auto pe_index = root.addr;
            auto &pe_root = pt[0];
            pagetable &ptl3 = pe_root.get_subtable();
            auto &pe = ptl3[pe_index];
            pe.present = 1;
            pe.writeable = 1;
            pe.user_access = 1;
            pe.write_through = 0;
            pe.cache_disabled = 0;
            pe.accessed = 0;
            pe.dirty = 0;
            pe.size = 0;
            pe.global = 0;
            pe.ignored2 = 0;
            pe.os_virt_avail = 0;
            pe.page_ppn = (root.physpage >> 12);
            pe.reserved1 = 0;
            pe.os_virt_start = 0;
            pe.ignored3 = 0;
            pe.execution_disabled = 0;
        }
    }
    for (auto &root : pagetableRoots) {
        if (root.addr < PMLT4_USERSPACE_HIGH_START ||
            root.addr >= PMLT4_USERSPACE_HIGH_END) {
            continue;
        }
        auto pe_index = root.addr;
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
        pe.ignored2 = 0;
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
    std::function<void (bool)> func;
};

static hw_spinlock pfault_lck{};
static raw_semaphore pfault_sema{-1};
static std::vector<process_pfault> faults{};
static std::thread *pfault_thread{nullptr};

void Process::activate_pfault_thread() {
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
                if (pfault.pfaultTask != nullptr) {
                    pfault.process->resolve_page_fault(*(pfault.pfaultTask), pfault.ip, pfault.address);
                } else {
                    pfault.process->resolve_read_page(pfault.address, pfault.func);
                }
            }
        });
    }
}

bool Process::page_fault(task &current_task, Interrupt &intr) {
    std::cout << "Page fault in user process with pagetable root " << std::hex << ((uintptr_t) get_root_pagetable())
    << " code " << intr.error_code() << std::dec << "\n";
    current_task.set_blocked(true);
    uint64_t address{0};
    asm("mov %%cr2, %0" : "=r"(address));
    std::lock_guard lock{pfault_lck};
    process_pfault pf{.pfaultTask = &current_task, .process = this, .ip = intr.rip(), .address = address, .func = [] (bool) {}};
    faults.push_back(pf);
    activate_pfault_thread();
    return true;
}

bool Process::exception(task &current_task, const std::string &name, Interrupt &intr) {
    std::cout << "Exception <"<< name <<"> in user process at " << std::hex << intr.rip() << " code " << intr.error_code() << std::dec << "\n";
    current_task.set_end(true);
    return true;
}

uintptr_t Process::push_64(uintptr_t ptr, uint64_t val, const std::function<void(bool, uintptr_t)> &func) {
    constexpr auto length = sizeof(val);
    uintptr_t dstptr{ptr - length};
    std::function<void (bool, uintptr_t)> f{func};
    resolve_read(dstptr, length, [this, dstptr, val, length, f] (bool success) mutable {
        if (!success) {
            f(false, 0);
        }
        auto offset = dstptr & ((uintptr_t) PAGESIZE-1);
        auto pageaddr = dstptr - offset;
        auto end = dstptr + length;
        vmem vm{end - pageaddr};
        auto firstPage = (pageaddr >> 12);
        for (int i = 0; i < vm.npages(); i++) {
            auto pe = Pageentry(firstPage + i);
            uintptr_t ph{pe->page_ppn};
            ph = ph << 12;
            vm.page(i).rwmap(ph);
        }
        void *ptr = (void *) ((uint8_t *) vm.pointer() + offset);
        *((uint64_t *) ptr) = val;
        f(true, dstptr);
    });
    return dstptr;
}

uintptr_t Process::push_data(uintptr_t ptr, const void *data, uintptr_t length, const std::function<void(bool, uintptr_t)> &func) {
    std::function<void (bool, uintptr_t)> f{func};
    if (length == 0) {
        f(true, ptr);
        return ptr;
    }
    uintptr_t dstptr{ptr - length};
    resolve_read(dstptr, length, [this, dstptr, data, length, f] (bool success) mutable {
        if (!success) {
            f(false, 0);
            return;
        }
        auto offset = dstptr & ((uintptr_t) PAGESIZE-1);
        auto pageaddr = dstptr - offset;
        auto end = dstptr + length;
        vmem vm{end - pageaddr};
        auto firstPage = (pageaddr >> 12);
        for (int i = 0; i < vm.npages(); i++) {
            auto pe = Pageentry(firstPage + i);
            uintptr_t ph{pe->page_ppn};
            ph = ph << 12;
            vm.page(i).rwmap(ph);
        }
        memcpy((uint8_t *) vm.pointer() + offset, data, length);
        f(true, dstptr);
    });
    return dstptr;
}

uintptr_t Process::push_string(uintptr_t ptr, const std::string &str, const std::function<void (bool, uintptr_t)> &func) {
    auto length = str.size() + 1;
    uintptr_t dstptr{ptr - length};
    std::string strcp{str};
    std::function<void (bool, uintptr_t)> f{func};
    resolve_read(dstptr, length, [this, dstptr, strcp, length, f] (bool success) mutable {
        if (!success) {
            f(false, 0);
            return;
        }
        auto offset = dstptr & ((uintptr_t) PAGESIZE-1);
        auto pageaddr = dstptr - offset;
        auto end = dstptr + length;
        vmem vm{end - pageaddr};
        auto firstPage = (pageaddr >> 12);
        for (int i = 0; i < vm.npages(); i++) {
            auto pe = Pageentry(firstPage + i);
            uintptr_t ph{pe->page_ppn};
            ph = ph << 12;
            vm.page(i).rwmap(ph);
        }
        memcpy((uint8_t *) vm.pointer() + offset, strcp.c_str(), length);
        f(true, dstptr);
    });
    return dstptr;
}

void Process::push_strings(uintptr_t ptr, const std::vector<std::string>::iterator &begin,
                           const std::vector<std::string>::iterator &end, const std::vector<uintptr_t> &strptrs,
                           const std::function<void(bool, const std::vector<uintptr_t> &, uintptr_t)> &func) {
    std::function<void(bool, const std::vector<uintptr_t> &, uintptr_t)> f{func};
    if (begin != end) {
        std::vector<std::string>::iterator b{begin};
        std::vector<std::string>::iterator e{end};
        std::vector<uintptr_t> strp{strptrs};
        push_string(ptr, *b, [this, b, e, f, strp] (bool success, uintptr_t ptr) mutable {
            if (!success) {
                f(false, strp, ptr);
                return;
            }
            strp.push_back(ptr);
            ++b;
            push_strings(ptr, b, e, strp, f);
        });
    } else {
        f(true, strptrs, ptr);
    }
}

bool Process::readable(uintptr_t addr) {
    std::lock_guard<hw_spinlock> lock{mtx};
    {
        constexpr phys_t userspaceLowEnd = ((phys_t) USERSPACE_LOW_END) << (9 + 9 + 12);
        constexpr phys_t userspaceHighStart = ((phys_t) PMLT4_USERSPACE_HIGH_START) << (9 + 9 + 9 + 12);
        uint32_t page;
        if (addr < userspaceHighStart) {
            if (addr >= userspaceLowEnd) {
                std::cerr << "User process page resolve: Outside userspace address space\n";
                return false;
            }
        }
        uintptr_t xaddr{addr};
        xaddr = xaddr >> 12;
        page = xaddr;
        MemMapping *mapping{nullptr};
        for (auto &m: mappings) {
            if (m.pagenum <= page && page < (m.pagenum + m.pages)) {
                mapping = &m;
                break;
            }
        }
        if (mapping == nullptr) {
            std::cerr << "User process page resolve: No mapping for page " << std::hex << page << std::dec << "\n";
            return false;
        }
        phys_t physpage = mapping->image_skip_pages + (page - mapping->pagenum);
        uint16_t rootAddr;
        {
            uint32_t addr{page};
            addr = addr >> (9 + 9 + 9);
            rootAddr = addr;
        }
        vmem vm{PAGESIZE};
        if (addr < userspaceHighStart) {
            PagetableRoot *root{nullptr};
            for (auto &r: pagetableLow) {
                if (r.addr == rootAddr) {
                    root = &r;
                    break;
                }
            }
            if (root == nullptr) {
                std::cerr << "User process page resolve: No low pagetable root for page " << std::hex << page << std::dec
                          << "\n";
                return false;
            }
            auto b2 = (*(root->branches))[(page >> 9) & 511];
            if (b2.present == 0) {
                std::cerr << "User process page resolve: Pagetable low directory not present for page " << std::hex << page
                          << std::dec << "\n";
                return false;
            }
            vm.page(0).rmap(b2.page_ppn << 12);
            vm.reload();
        } else {
            PagetableRoot *root{nullptr};
            for (auto &r: pagetableRoots) {
                if (r.addr == rootAddr) {
                    root = &r;
                    break;
                }
            }
            if (root == nullptr) {
                std::cerr << "User process page resolve: No pagetable root for page " << std::hex << page << std::dec
                          << "\n";
                return false;
            }
            auto b1 = (*(root->branches))[(page >> (9 + 9)) & 511];
            if (b1.present == 0) {
                std::cerr << "User process page resolve: Pagetable directory not present for page " << std::hex << page
                          << std::dec << "\n";
                return false;
            }
            vm.page(0).rmap(b1.page_ppn << 12);
            vm.reload();
            auto b2 = (*((pagetable *) vm.pointer()))[(page >> 9) & 511];
            if (b2.present == 0) {
                std::cerr << "User process page resolve: Pagetable not present for page " << std::hex << page
                          << std::dec
                          << "\n";
                return false;
            }
            vm.page(0).rwmap(b2.page_ppn << 12);
            vm.reload();
        }
        auto b3 = (*((pagetable *) vm.pointer()))[page & 511];
        return b3.present != 0 && b3.user_access != 0;
    }
}

bool Process::resolve_page(uintptr_t fault_addr) {
    std::unique_ptr<std::lock_guard<hw_spinlock>> lock{new std::lock_guard(mtx)};
    constexpr uint64_t highRelocationOffset = ((uint64_t) PMLT4_USERSPACE_HIGH_START) << (9 + 9 + 9 + 12);
    uint32_t page;
    if (fault_addr < highRelocationOffset) {
        constexpr phys_t lowUserpaceMax = ((phys_t) USERSPACE_LOW_END) << (9+9+12);
        if (fault_addr >= lowUserpaceMax) {
            std::cerr << "User process page resolve: Not within userpace memory limits\n";
            return false;
        }
    }
    {
        uintptr_t addr{fault_addr};
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
        std::cerr << "User process page resolve: No mapping for page " << std::hex << page << std::dec << "\n";
        return false;
    }
    phys_t physpage = mapping->image_skip_pages + (page - mapping->pagenum);
    uint16_t rootAddr;
    vmem vm{PAGESIZE};
    pageentr *b2;
    PagetableRoot *root{nullptr};
    if (fault_addr < highRelocationOffset) {
        {
            uint32_t addr{page};
            addr = addr >> (9+9);
            rootAddr = addr;
        }
        for (auto &r: pagetableLow) {
            if (r.addr == rootAddr) {
                root = &r;
                break;
            }
        }
        if (root == nullptr) {
            std::cerr << "User process page resolve: No low pagetable root for page " << std::hex << page << std::dec << "\n";
            return false;
        }
        b2 = &((*(root->branches))[(page >> 9) & 511]);
        if (b2->present == 0) {
            std::cerr << "User process page resolve: Pagetable directory not present for page " << std::hex << page << std::dec << "\n";
            return false;
        }
    } else {
        {
            uint32_t addr{page};
            addr = addr >> (9+9+9);
            rootAddr = addr;
        }
        for (auto &r: pagetableRoots) {
            if (r.addr == rootAddr) {
                root = &r;
                break;
            }
        }
        if (root == nullptr) {
            std::cerr << "User process page resolve: No pagetable root for page " << std::hex << page << std::dec << "\n";
            return false;
        }
        auto b1 = (*(root->branches))[(page >> (9+9)) & 511];
        if (b1.present == 0) {
            std::cerr << "User process page resolve: Pagetable directory not present for page " << std::hex << page << std::dec << "\n";
            return false;
        }
        vm.page(0).rmap(b1.page_ppn << 12);
        vm.reload();
        b2 = &((*((pagetable *) vm.pointer()))[(page >> 9) & 511]);
        if (b2->present == 0) {
            std::cerr << "User process page resolve: Pagetable not present for page " << std::hex << page << std::dec << "\n";
            return false;
        }
    }
    vm.page(0).rwmap(b2->page_ppn << 12);
    vm.reload();
    auto &b3 = (*((pagetable *) vm.pointer()))[page & 511];
    if (b3.present == 0) {
        std::cout << "Loading page " << std::hex << page << " for addr " << fault_addr << " from " << physpage << std::dec << "\n";
        lock = {};
        auto image = mapping->image;
        phys_t phys_page;
        filepage_ref loaded_page{};
        if (image) {
            loaded_page = image->GetPage(physpage);
            phys_page = loaded_page.PhysAddr();
        } else {
            phys_page = ppagealloc(PAGESIZE);
        }
        if (phys_page == 0) {
            std::cerr << "User process page resolve: Failed to allocate phys page\n";
            return false;
        }
        lock = std::make_unique<std::lock_guard<hw_spinlock>>(mtx);
        bool ok{false};
        for (auto &m : mappings) {
            if (m.pagenum <= page && page < (m.pagenum + m.pages)) {
                if (m.image) {
                    if (mapping != &m || physpage != (m.image_skip_pages + (page - m.pagenum)) || image != m.image) {
                        std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page
                                  << std::dec << "\n";
                        if (!loaded_page) {
                            ppagefree(phys_page, PAGESIZE);
                        }
                        return false;
                    }
                } else {
                    if (mapping != &m || loaded_page) {
                        std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page
                                  << std::dec << "\n";
                        if (!loaded_page) {
                            ppagefree(phys_page, PAGESIZE);
                        }
                        return false;
                    }
                }
                ok = true;
                break;
            }
        }
        if (!ok) {
            std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page << std::dec << "\n";
            if (!loaded_page) {
                ppagefree(phys_page, PAGESIZE);
            }
            return false;
        }
        ok = false;
        pageentr *b2;
        if (fault_addr < highRelocationOffset) {
            for (auto &r: pagetableLow) {
                if (r.addr == rootAddr) {
                    if (root != &r) {
                        std::cerr << "User process page resolve: Pagetable root was removed for page " << std::hex
                                  << page << std::dec << "\n";
                        if (!loaded_page) {
                            ppagefree(phys_page, PAGESIZE);
                        }
                        return false;
                    }
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                std::cerr << "User process page resolve: Pagetable low root was removed for page " << std::hex << page
                          << std::dec << "\n";
                if (!loaded_page) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            b2 = &((*(root->branches))[(page >> 9) & 511]);
            std::cout << "Low root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(*b2))) << std::dec << "\n";
            if (b2->present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
        } else {
            for (auto &r: pagetableRoots) {
                if (r.addr == rootAddr) {
                    if (root != &r) {
                        std::cerr << "User process page resolve: Pagetable root was removed for page " << std::hex
                                  << page << std::dec << "\n";
                        if (!loaded_page) {
                            ppagefree(phys_page, PAGESIZE);
                        }
                        return false;
                    }
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                std::cerr << "User process page resolve: Pagetable root was removed for page " << std::hex << page
                          << std::dec << "\n";
                if (!loaded_page) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            auto &b1 = (*(root->branches))[(page >> (9+9)) & 511];
            std::cout << "Root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(b1))) << std::dec << "\n";
            if (b1.present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            vm.page(0).rmap(b1.page_ppn << 12);
            vm.reload();
            b2 = &((*((pagetable *) vm.pointer()))[(page >> 9) & 511]);
            if (b2->present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
        }
        auto b2phys = b2->page_ppn << 12;
        vm.page(0).rwmap(b2phys);
        vm.reload();
        auto &b3 = (*((pagetable *) vm.pointer()))[page & 511];
        if (b3.present == 0) {
            std::cout << std::hex << ((b2phys) + (page & 511)) << " *" << (b3.user_access != 0 ? "u" : "k") << (b3.writeable != 0 ? "w" : "r")
                      << (b3.execution_disabled != 0 ? "n" : "e") << " " << b3.page_ppn << std::dec
                      << "\n";
            mapping->mappings.push_back({.data = loaded_page, .phys_page = phys_page, .page = page});
            b3.page_ppn = phys_page >> 12;
            b3.present = 1;
            b3.user_access = 1;
            reload_pagetables();
        }
        return true;
    } else if (mapping->cow) {
        for (auto &pagemap : mapping->mappings) {
            if (page == pagemap.page && pagemap.data) {
                auto phys = ppagealloc(PAGESIZE);
                if (phys == 0) {
                    std::cerr << "Error: Unable to allocate phys page for copy on write\n";
                    return false;
                }
                std::cout << "Memory copy " << std::hex << page << ": " << pagemap.data.PhysAddr() << " -> " << phys << "\n";
                vmem vm{PAGESIZE * 2};
                vm.page(0).rwmap(phys);
                vm.page(1).rmap(pagemap.data.PhysAddr());
                vm.reload();
                uint8_t *src = (uint8_t *) vm.pointer();
                src += PAGESIZE;
                memcpy(vm.pointer(), src, PAGESIZE);
                b3.user_access = 1;
                b3.page_ppn = phys >> 12;
                b3.writeable = 1;
                b3.execution_disabled = 1;
                b3.present = 1;
                vm.reload();
                pagemap.data = {};
                pagemap.phys_page = phys;
                return true;
            }
        }
    }
    return false;
}

void Process::resolve_page_fault(task &current_task, uintptr_t ip, uintptr_t fault_addr) {
    if (resolve_page(fault_addr)) {
        current_task.set_blocked(false);
        return;
    }
    std::cerr << "PID " << current_task.get_id() << ": Page fault at " << std::hex << ip << " addr " << fault_addr << std::dec << "\n";
    {
        auto &cpu = current_task.get_cpu_frame();
        auto &state = current_task.get_cpu_state();
        std::cerr << std::hex << "ax=" << state.rax << " bx="<<state.rbx << " cx=" << state.rcx << " dx="<< state.rdx << "\n"
                 << "si=" << state.rsi << " di="<< state.rdi << " bp=" << state.rbp << " sp="<< cpu.rsp << "\n"
                 << "r8=" << state.r8 << " r9="<< state.r9 << " rA="<< state.r10 <<" rB=" << state.r11 << "\n"
                 << "rC=" << state.r12 << " rD="<< state.r13 <<" rE="<< state.r14 <<" rF="<< state.r15 << "\n"
                 << "cs=" << cpu.cs << " ds=" << state.ds << " es=" << state.es << " fs=" << state.fs << " gs="
                 << state.gs << " ss="<< cpu.ss <<"\n"
                 << "fsbase=" << state.fsbase << "\n" << std::dec;
    }
    get_scheduler()->terminate_blocked(&current_task);
}

void Process::resolve_read_page(uintptr_t addr, std::function<void(bool)> func) {
    if (resolve_page(addr)) {
        func(true);
    } else {
        func(false);
    }
}

void Process::resolve_read(uintptr_t addr, uintptr_t len, std::function<void(bool)> func) {
    uintptr_t end = addr + len;
    if (addr == end) {
        func(true);
        return;
    }
    while (readable(addr)) {
        addr += 4096;
        len -= 4096;
        uintptr_t next_addr = addr & ~((uintptr_t) 4095);
        if (next_addr >= end) {
            func(true);
            return;
        }
    }
    std::lock_guard lock{pfault_lck};
    faults.push_back({.pfaultTask = nullptr, .process = this, .ip = 0, .address = addr, .func = [this, addr, len, end, func] (bool success) mutable {
        if (!success) {
            func(false);
        } else {
            addr += 4096;
            len -= 4096;
            uintptr_t next_addr = addr & ~((uintptr_t) 4095);
            if (next_addr < end) {
                resolve_read(addr, len, func);
            } else {
                func(true);
            }
        }
    }});
    activate_pfault_thread();
}

FileDescriptor Process::get_file_descriptor(int fd) {
    for (auto desc : fileDescriptors) {
        if (desc.FD() == fd) {
            return desc;
        }
    }
    return {};
}

bool Process::brk(intptr_t delta_addr, uintptr_t &result) {
    constexpr uint64_t highEnd = ((uint64_t) PMLT4_USERSPACE_HIGH_END) << (9 + 9 + 9 + 12);
    std::lock_guard lock{mtx};
    MemMapping *mapping{nullptr};
    for (auto &m : mappings) {
        if (!m.binary_mapping) {
            continue;
        }
        if (mapping == nullptr) {
            mapping = &m;
            continue;
        }
        auto mappingEnd = m.pagenum + m.pages;
        auto compMappingEnd = mapping->pagenum + mapping->pages;
        if (mappingEnd > compMappingEnd) {
            mapping = &m;
        }
    }
    if (mapping == nullptr) {
        return false;
    }
    if (mapping->image) {
        return false;
    }
    uintptr_t addr = mapping->pagenum;
    addr += mapping->pages;
    addr = addr << 12;
    addr += delta_addr;
    if (addr > highEnd) {
        return false;
    }
    result = addr;
    if constexpr(USERSPACE_LOW_END != 512 || PMLT4_USERSPACE_HIGH_START != 1) {
        constexpr uint64_t lowEnd = ((uint64_t) USERSPACE_LOW_END) << (9 + 9 + 12);
        uintptr_t end = (uintptr_t) mapping->pagenum + mapping->pages;
        end = end << 12;
        if (end <= lowEnd && addr > lowEnd) {
            return false;
        }
    }
    auto page = addr >> 12;
    if ((addr & (PAGESIZE-1)) != 0) {
        ++page;
    }
    if (mapping->pagenum >= page) {
        return false;
    }
    uintptr_t endpage = (uintptr_t) mapping->pagenum + mapping->pages;
    if (endpage == page) {
        return true;
    }
    if (endpage < page) {
        /* extend */
        if (!CheckMapOverlap(endpage, page - endpage)) {
            return false;
        }
        mapping->pages = page - mapping->pagenum;
        for (auto p = endpage; p < page; p++) {
            bool success = Pageentry(p, [] (pageentr &pe) {
                pe.present = 0;
                pe.writeable = 1;
                pe.user_access = 1;
                pe.write_through = 0;
                pe.cache_disabled = 0;
                pe.accessed = 0;
                pe.dirty = 0;
                pe.size = 0;
                pe.global = 0;
                pe.ignored2 = 0;
                pe.os_virt_avail = 0;
                pe.page_ppn = 0;
                pe.reserved1 = 0;
                pe.os_virt_start = 0;
                pe.ignored3 = 0;
                pe.execution_disabled = 0;
            });
            if (!success) {
                mapping->pages = endpage - mapping->pagenum;
                return false;
            }
        }
    } else {
        /* shrink */
        mapping->pages = page - mapping->pagenum;
        for (auto p = page; p < endpage; p++) {
            bool success = Pageentry(p, [] (pageentr &pe) {
                pe.present = 0;
            });
            if (success) {
                auto iterator = mapping->mappings.begin();
                while (iterator != mapping->mappings.end()) {
                    auto &pmap = *iterator;
                    if (p == pmap.page) {
                        ppagefree(pmap.phys_page, PAGESIZE);
                        mapping->mappings.erase(iterator);
                    } else {
                        ++iterator;
                    }
                }
            } else {
                mapping->pages = endpage - mapping->pagenum;
                return false;
            }
        }
    }
    return true;
}