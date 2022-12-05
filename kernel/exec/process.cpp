//
// Created by sigsegv on 6/8/22.
//

#include <sys/types.h>
#include <sys/mman.h>
#include <iostream>
#include <exec/process.h>
#include <exec/procthread.h>
#include <pagealloc.h>
#include <strings.h>
#include <mutex>
#include <thread>
#include <errno.h>
#include <kfs/kfiles.h>
#include "concurrency/raw_semaphore.h"
#include "StdinDesc.h"
#include "StdoutDesc.h"

//#define DEBUG_PROCESS_PAGEENTR
//#define DEBUG_PAGE_FAULT_RESOLVE
//#define DEBUG_MAP_CLEAR_FREE
//#define DEBUG_MEMMAPPING_DESTRUCTION_FREEPAGE
//#define DEBUG_MMAP
//#define DEBUG_MMAP_OVERLAP

static hw_spinlock pidLock{};
static std::vector<pid_t> pids{};
static pid_t next = 1;

PagetableRoot::PagetableRoot(uint16_t addr, bool twolevel) : physpage(ppagealloc(PAGESIZE)), vm(PAGESIZE), branches((pagetable *) vm.pointer()), addr(addr), twolevel(twolevel) {
    if (physpage != 0) {
        vm.page(0).rwmap(physpage);
        bzero(branches, sizeof(*branches));
    }
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
                if (twolevel) {
                    for (int j = 0; j < 512; j++) {
                        auto &b2 = (*table)[j];
                        if (b2.page_ppn != 0) {
                            ppagefree((b2.page_ppn << 12), sizeof(*table));
                        }
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
    if (this != &mv) {
        mv.physpage = 0;
    }
}

MemMapping::MemMapping() : image(), pagenum(0), pages(0), image_skip_pages(0), load(0), read(false), cow(false), binary_mapping(false), mappings() {}

void MemMapping::CopyAttributes(MemMapping &dst, const MemMapping &src) {
    dst.image = src.image;
    dst.pagenum = src.pagenum;
    dst.pages = src.pages;
    dst.image_skip_pages = src.image_skip_pages;
    dst.load = src.load;
    dst.read = src.read;
    dst.cow = src.cow;
    dst.binary_mapping = src.binary_mapping;
}

MemMapping::MemMapping(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool cow, bool binaryMapping)
: image(image), pagenum(pagenum), pages(pages), image_skip_pages(image_skip_pages), load(load), read(true), cow(cow), binary_mapping(binaryMapping), mappings() {}

MemMapping::MemMapping(MemMapping &&mv) : image(mv.image), pagenum(mv.pagenum), pages(mv.pages), image_skip_pages(mv.image_skip_pages), load(mv.load), read(mv.read), cow(mv.cow), binary_mapping(mv.binary_mapping), mappings(mv.mappings) {
    mv.mappings.clear();
}

MemMapping &MemMapping::operator =(MemMapping &&mv) {
    if (this == &mv) {
        return *this;
    }
    image = mv.image;
    pagenum = mv.pagenum;
    pages = mv.pages;
    image_skip_pages = mv.image_skip_pages;
    load = mv.load;
    read = mv.read;
    cow = mv.cow;
    binary_mapping = mv.binary_mapping;
    mappings = mv.mappings;
    mv.mappings.clear();
    return *this;
}

MemMapping::~MemMapping() {
#ifdef DEBUG_MEMMAPPING_DESTRUCTION_FREEPAGE
    bool freePages{false};
#endif
    for (const auto &ppage : mappings) {
        if (!ppage.data && !ppage.cow) {
#ifdef DEBUG_MEMMAPPING_DESTRUCTION_FREEPAGE
            if (!freePages) {
                std::cout << "Free pages: ";
                freePages = true;
            }
            std::cout << " " << std::hex << ppage.phys_page << std::dec;
#endif
            phys_t addr = ppage.phys_page;
            ppagefree(addr, PAGESIZE);
        }
    }
#ifdef DEBUG_MEMMAPPING_DESTRUCTION_FREEPAGE
    if (freePages) {
        std::cout << "\n";
    }
#endif
}

DeferredReleasePage::DeferredReleasePage(DeferredReleasePage &&mv) : pagenum(mv.pagenum), data(mv.data), cow(mv.cow) {
    if (this != &mv) {
        mv.pagenum = 0;
        mv.data = {};
        mv.cow = {};
    }
}

DeferredReleasePage::~DeferredReleasePage() {
    if (!data && !cow && pagenum != 0) {
        uintptr_t addr{pagenum};
        addr = addr << 12;
        ppagefree(addr, PAGESIZE);
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

Process::Process(const std::shared_ptr<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid, const std::string &cmdline) : mtx(), self_ref(), sigmask(), rlimits(), pid(0), pgrp(0), parent(), parent_pid(parent_pid), pagetableLow(), pagetableRoots(), mappings(), fileDescriptors(), relocations(), cwd(cwd), cmdline(cmdline), tty(tty), sigactions(), exitNotifications(), childExitNotifications(), exitCode(-1), program_brk(0), euid(0), egid(0), uid(0), gid(0), fwaits() {
    fileDescriptors.push_back(StdinDesc::Descriptor(tty));
    fileDescriptors.push_back(StdoutDesc::StdoutDescriptor());
    fileDescriptors.push_back(StdoutDesc::StderrDescriptor());
    pid = AllocPid();
    pgrp = pid;
}

Process::Process(std::shared_ptr<Process> cp) : mtx(), self_ref(), sigmask(cp->sigmask), rlimits(cp->rlimits), pid(0), pgrp(cp->pgrp), parent(cp), parent_pid(cp->pid), pagetableLow(), pagetableRoots(), mappings(), fileDescriptors(), relocations(), cwd(cp->cwd), cmdline(cp->cmdline), tty(cp->tty), sigactions(cp->sigactions), exitNotifications(), childExitNotifications(), exitCode(-1), program_brk(cp->program_brk), euid(cp->euid), egid(cp->egid), uid(cp->uid), gid(cp->gid), fwaits() {
    pid = AllocPid();
}

std::shared_ptr<Process>
Process::Create(const std::shared_ptr<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid,
                const std::string &cmdline) {
    std::shared_ptr<Process> process{new Process(cwd, tty, parent_pid, cmdline)};
    std::weak_ptr<Process> weak{process};
    process->self_ref = weak;
    return process;
}

Process::~Process() {
    std::vector<std::function<void (intptr_t)>> calls{};
    {
        std::lock_guard lock{mtx};
        for (auto en: exitNotifications) {
            calls.push_back(en);
        }
        exitNotifications.clear();
    }
    for (auto en : calls) {
        en(exitCode);
    }
    {
        std::shared_ptr<Process> parentRef{};
        parentRef = parent.lock();
        if (parentRef) {
            parentRef->ChildExitNotification(pid, exitCode);
        }
    }
}

void Process::ChildExitNotification(pid_t cpid, intptr_t status) {
    std::function<void (pid_t, intptr_t)> notify{};
    {
        std::lock_guard lock{mtx};
        if (childExitNotifications.empty()) {
            childResults.push_back({.result = status, .pid = cpid});
            return;
        }
        auto iterator = childExitNotifications.begin();
        notify = *iterator;
        childExitNotifications.erase(iterator);
    }
    notify(cpid, status);
}

bool Process::WaitForAnyChild(child_result &immediateResult, const std::function<void(pid_t, intptr_t)> &orAsync) {
    std::lock_guard lock{mtx};
    if (childResults.empty()) {
        childExitNotifications.push_back(orAsync);
        return false;
    }
    auto iterator = childResults.begin();
    immediateResult = *iterator;
    childResults.erase(iterator);
    return true;
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
            root = &(pagetableLow.emplace_back(lowRoot, false));
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
            root = &(pagetableRoots.emplace_back(index, true));
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

    uint32_t Length() const {
        return end - start;
    }
    MemoryArea Combine(const MemoryArea &other) const {
        MemoryArea area{other};
        if (area.start > start) {
            area.start = start;
        }
        if (area.end < end) {
            area.end = end;
        }
        return area;
    }
    bool Overlaps(const MemoryArea &other) const {
        return (Combine(other).Length() < (Length() + other.Length()));
    }
};

bool Process::IsFree(uint32_t pagenum, uint32_t pages) {
    MemoryArea area{.start = pagenum, .end = (pagenum + pages)};
    for (const auto &mapping : mappings) {
        MemoryArea mappingArea{.start = mapping.pagenum, .end = (mapping.pagenum + mapping.pages)};
        if (area.Overlaps(mappingArea)) {
            return false;
        }
    }
    return true;
}

constexpr uint32_t lowLim = ((uintptr_t) PMLT4_USERSPACE_HIGH_START) << (9 + 9 + 9);
constexpr uintptr_t highLimBase = ((uintptr_t) PMLT4_USERSPACE_HIGH_END) << (9 + 9 + 9);
constexpr uint32_t highLim = highLimBase < 0x100000000 ? highLimBase : 0xFFFFFFFF;

bool Process::IsInRange(uint32_t pagenum, uint32_t pages) {
    uintptr_t start{pagenum};
    uintptr_t end{pagenum};
    end += pages;
    if (start < lowLim) {
        // TODO - low address space
        return false;
    }
    return (start >= lowLim) && (end <= highLim);
}

void Process::DisableRange(uint32_t pagenum, uint32_t pages) {
    for (typeof(pagenum) p = 0; p < pages; p++) {
        auto addr = pagenum;
        addr += p;
        Pageentry(addr, [] (pageentr &pe) {
            pe.present = 0;
        });
    }
}

std::vector<DeferredReleasePage> Process::ClearRange(uint32_t pagenum, uint32_t pages) {
    std::vector<DeferredReleasePage> release{};
    release.reserve(2);
    std::lock_guard lock{mtx};
    auto iterator = mappings.begin();
    std::vector<MemMapping> newMappings{};
    while (iterator != mappings.end()) {
        auto &mapping = *iterator;
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
                auto mapping_end = mapping.pagenum + mapping.pages;
                if (mapping.pagenum < overlap_page) {
                    if (mapping_end > overlap_end) {
                        auto &newMapping = newMappings.emplace_back();
                        MemMapping::CopyAttributes(newMapping, mapping);
#ifdef DEBUG_MMAP_OVERLAP
                        std::cout << "Map split " << std::hex << mapping.pagenum << "/" << mapping.pages;
#endif
                        mapping.pages = overlap_page - mapping.pagenum;
#ifdef DEBUG_MMAP_OVERLAP
                        std::cout << " -> " << std::hex << mapping.pagenum << "/" << mapping.pages << ", ";
#endif
                        newMapping.pages -= overlap_end - mapping.pagenum;
                        newMapping.pagenum = overlap_end;
#ifdef DEBUG_MMAP_OVERLAP
                        std::cout << newMapping.pagenum << "/" << newMapping.pages << "\n";
#endif
                        {
#ifdef DEBUG_MAP_CLEAR_FREE
                            std::cout << "Free pages: ";
#endif
                            auto dropIterator = mapping.mappings.begin();
                            while (dropIterator != mapping.mappings.end()) {
                                const auto &check = *dropIterator;
                                if (check.page >= overlap_page) {
                                    if (check.page < overlap_end) {
                                        release.emplace_back(check.phys_page, check.data, check.cow);
                                    } else {
#ifdef DEBUG_MAP_CLEAR_FREE
                                        if (!check.data && check.phys_page != 0) {
                                            std::cout << " *" << std::hex << check.page << std::dec;
                                        }
#endif
                                        newMapping.mappings.push_back(check);
                                    }
                                    mapping.mappings.erase(dropIterator);
                                } else {
                                    ++dropIterator;
                                }
                            }
#ifdef DEBUG_MAP_CLEAR_FREE
                            std::cout << ".\n";
#endif
                        }
                    } else {
#ifdef DEBUG_MMAP_OVERLAP
                        std::cout << "Map slit/2 type-reduce keep:" << std::hex << mapping.pagenum << "-" << overlap_page << " drop:-" << overlap_end << std::dec << "\n";
#endif
                        mapping.pages = overlap_page - mapping.pagenum;
                        {
                            auto dropIterator = mapping.mappings.begin();
#ifdef DEBUG_MAP_CLEAR_FREE
                            std::cout << "Free pages: ";
#endif
                            while (dropIterator != mapping.mappings.end()) {
                                auto &check = *dropIterator;
                                if (check.page >= overlap_page) {
                                    release.emplace_back(check.phys_page, check.data, check.cow);
                                    mapping.mappings.erase(dropIterator);
                                } else {
                                    ++dropIterator;
                                }
                            }
#ifdef DEBUG_MAP_CLEAR_FREE
                            std::cout << ".\n";
#endif
                        }
                    }
                } else if (mapping_end > overlap_end) {
#ifdef DEBUG_MMAP_OVERLAP
                    std::cout << "Map slit/3 type-reduce drop:" << std::hex << mapping.pagenum << "- keep:" << overlap_end << "-" << mapping_end << std::dec << "\n";
#endif
                    mapping.pages = mapping_end - overlap_end;
                    mapping.pagenum = overlap_end;
                    {
#ifdef DEBUG_MAP_CLEAR_FREE
                        std::cout << "Free pages: ";
#endif
                        auto dropIterator = mapping.mappings.begin();
                        while (dropIterator != mapping.mappings.end()) {
                            auto &check = *dropIterator;
                            if (check.page < overlap_end) {
                                release.emplace_back(check.phys_page, check.data, check.cow);
                                mapping.mappings.erase(dropIterator);
                            } else {
                                ++dropIterator;
                            }
                        }
#ifdef DEBUG_MAP_CLEAR_FREE
                        std::cout << ".\n";
#endif
                    }
                } else {
#ifdef DEBUG_MMAP_OVERLAP
                    std::cout << "Map slit/4 type-drop drop:" << std::hex << mapping.pagenum << " num:" << mapping.pages << std::dec << "\n";
#endif
                    mappings.erase(iterator);
                    continue;
                }
            }
        }
        ++iterator;
    }
    for (auto &mapping : newMappings) {
        auto &map = mappings.emplace_back();
        MemMapping::CopyAttributes(map, mapping);
        map.mappings.reserve(mapping.mappings.size());
        for (const auto &m : mapping.mappings) {
            map.mappings.push_back(m);
        }
        mapping.mappings.clear();
    }
    return release;
}

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

void Process::TearDownMemory() {
    std::lock_guard lock{mtx};
    for (auto &mapping : mappings) {
        for (auto &phmap : mapping.mappings) {
            Pageentry(phmap.page, [] (pageentr &pe) {
                pe.present = 0;
                pe.writeable = 0;
                pe.execution_disabled = 1;
                pe.page_ppn = 0;
            });
        }
    }
    mappings.clear();
}

std::vector<MemMapping> Process::WriteProtectCow() {
    std::vector<MemMapping> memMappings{};
    std::lock_guard lock{mtx};
    for (auto &mapping : mappings) {
        auto &newMapping = memMappings.emplace_back(mapping.image, mapping.pagenum, mapping.pages, mapping.image_skip_pages, mapping.load, mapping.cow, mapping.binary_mapping);
        newMapping.read = mapping.read;
        for (auto &phmap : mapping.mappings) {
            bool wasWriteable{false};
            Pageentry(phmap.page, [&wasWriteable] (pageentr &pe) {
                if (pe.writeable != 0) {
                    wasWriteable = true;
                    pe.writeable = 0;
                }
            });
            if (wasWriteable || phmap.data || phmap.cow) {
                if (phmap.phys_page != 0 && !phmap.data && !phmap.cow) {
                    phmap.cow = std::make_shared<CowPageRef>(phmap.phys_page);
                    phmap.phys_page = 0;
                }
                auto &newPhysMapping = newMapping.mappings.emplace_back();
                newPhysMapping.phys_page = phmap.phys_page;
                newPhysMapping.page = phmap.page;
                newPhysMapping.data = phmap.data;
                newPhysMapping.cow = phmap.cow;
            }
        }
    }
    return memMappings;
}

std::shared_ptr<Process> Process::Clone() {
    std::shared_ptr<Process> clonedProcess{};
    std::shared_ptr<Process> original{};
    original = this->self_ref.lock();
    {
        std::lock_guard lock{mtx};
        clonedProcess = new Process(original);
    }
    {
        std::weak_ptr<Process> weak{clonedProcess};
        clonedProcess->self_ref = weak;
    }
    clonedProcess->mappings = WriteProtectCow();
    for (auto &mapping : clonedProcess->mappings)
    {
        uintptr_t pageend = mapping.pagenum;
        pageend += mapping.pages;
        for (uintptr_t pageaddr = mapping.pagenum; pageaddr < pageend; pageaddr++) {
            auto srcPe = Pageentry(pageaddr);
            if (srcPe) {
                clonedProcess->Pageentry(pageaddr, [&srcPe] (pageentr &pe) {
                    pe = *srcPe;
                });
            }
        }
    }
    for (const auto &fd : fileDescriptors) {
        clonedProcess->fileDescriptors.push_back(fd);
    }
    return clonedProcess;
}

bool Process::Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap) {
#ifdef DEBUG_MMAP
    std::cout << "Map " << std::hex << pagenum << "-" << (pagenum+pages) << " -> " << image_skip_pages << (write ? " write" : "") << (execute ? " exec" : "") << std::dec << "\n";
#endif
    if (write && !copyOnWrite) {
        std::cerr << "Error mapping: Write without COW is not supported\n";
    }
    std::lock_guard lock{mtx};
    if (!CheckMapOverlap(pagenum, pages)) {
        std::cerr << "Map error: overlapping existing mapping\n";
        return false;
    }
    mappings.emplace_back(image, pagenum, pages, image_skip_pages, load, write && copyOnWrite, binaryMap);
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
#ifdef DEBUG_MMAP
    std::cout << "Map " << std::hex << pagenum << "-" << (pagenum+pages) << " on demand " << std::dec << "\n";
#endif
    std::lock_guard lock{mtx};
    if (!CheckMapOverlap(pagenum, pages)) {
        return false;
    }
    mappings.emplace_back(std::shared_ptr<kfile>(), pagenum, pages, 0, 0, false, binaryMap);
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

struct ProtectMapping {
    uint32_t start, end;
    MemMapping *mapping;
};

int Process::Protect(uint32_t pagenum, uint32_t pages, int prot) {
    uint32_t covered{0};
    std::vector<ProtectMapping> prot_mappings{};
    std::lock_guard lock{mtx};
    prot_mappings.reserve(4);
    for (auto &mapping : mappings) {
        auto ol_start = mapping.pagenum;
        if (ol_start < pagenum) {
            ol_start = pagenum;
        }
        auto ol_end = mapping.pagenum + mapping.pages;
        if (ol_end > (pagenum + pages)) {
            ol_end = pagenum + pages;
        }
        if (ol_start >= ol_end) {
            continue;
        }
        ProtectMapping p{.start = ol_start, .end = ol_end, .mapping = &mapping};
        prot_mappings.push_back(p);
        covered += ol_end - ol_start;
    }
    if (covered < pages) {
        std::cerr << "mprotect: not all pages are mapped\n";
        return -EINVAL;
    }
    std::vector<MemMapping> new_mappings{};
    new_mappings.reserve(prot_mappings.size() * 2);
    std::function<void ()> apply_new_mappings = [this, &new_mappings] () {
        mappings.reserve(mappings.size() + new_mappings.size());
        for (auto &mapping : new_mappings) {
            auto &map = mappings.emplace_back();
            MemMapping::CopyAttributes(map, mapping);
            for (const auto &m : mapping.mappings) {
                map.mappings.push_back(m);
            }
            mapping.mappings.clear();
        }
    };
    for (auto &pmapping : prot_mappings) {
        if (pmapping.start > pmapping.mapping->pagenum) {
            auto size_pages = pmapping.start - pmapping.mapping->pagenum;
            auto &map = new_mappings.emplace_back();
            MemMapping::CopyAttributes(map, *(pmapping.mapping));
            map.pages = size_pages;
            pmapping.mapping->pagenum = pmapping.start;
            pmapping.mapping->pages -= size_pages;
            if (pmapping.mapping->image) {
                pmapping.mapping->image_skip_pages += size_pages;
            }
            auto iterator = pmapping.mapping->mappings.begin();
            while (iterator != pmapping.mapping->mappings.end()) {
                auto &mapping = *iterator;
                if (mapping.page >= map.pagenum && mapping.page < (map.pagenum + map.pages)) {
                    map.mappings.push_back((const PhysMapping &) mapping);
                    pmapping.mapping->mappings.erase(iterator);
                    continue;
                }
                ++iterator;
            }
        }
        if (pmapping.end < (pmapping.mapping->pagenum + pmapping.mapping->pages)) {
            auto size_pages1 = pmapping.end - pmapping.mapping->pagenum;
            auto size_pages2 = pmapping.mapping->pages - size_pages1;
            auto &map = new_mappings.emplace_back(pmapping.mapping->image, pmapping.mapping->pagenum + size_pages1, size_pages2, pmapping.mapping->image_skip_pages + size_pages1, 0, pmapping.mapping->cow, pmapping.mapping->binary_mapping);
            pmapping.mapping->pages = size_pages1;
            auto iterator = pmapping.mapping->mappings.begin();
            while (iterator != pmapping.mapping->mappings.end()) {
                auto &mapping = *iterator;
                if (mapping.page >= map.pagenum && mapping.page < (map.pagenum + map.pages)) {
                    map.mappings.push_back((const PhysMapping &) mapping);
                    pmapping.mapping->mappings.erase(iterator);
                    continue;
                }
                ++iterator;
            }
        }
        if (prot == PROT_READ) {
            auto end = pmapping.mapping->pagenum + pmapping.mapping->pages;
            for (auto p = pmapping.mapping->pagenum; p < end; p++) {
                Pageentry(p, [](pageentr &pe) {
                    pe.execution_disabled = 1;
                    pe.writeable = 0;
                });
            }
            pmapping.mapping->read = true;
            pmapping.mapping->cow = false;
        } else if (prot == PROT_NONE) {
            auto end = pmapping.mapping->pagenum + pmapping.mapping->pages;
            for (auto p = pmapping.mapping->pagenum; p < end; p++) {
                Pageentry(p, [](pageentr &pe) {
                    pe.execution_disabled = 1;
                    pe.writeable = 0;
                    pe.present = 0;
                });
            }
            pmapping.mapping->read = false;
            pmapping.mapping->cow = false;
        } else {
            std::cerr << "mprotect: invalid prot let through\n";
            apply_new_mappings();
            return -EINVAL;
        }
    }
    apply_new_mappings();
    return 0;
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
    ProcThread *process;
    uintptr_t ip;
    uintptr_t address;
    std::function<void (bool)> func;
};

static hw_spinlock pfault_lck{};
static raw_semaphore pfault_sema{-1};
static std::vector<process_pfault> p_faults{};
static std::thread *pfault_thread{nullptr};
static hw_spinlock pfault_thread_activate_single{};

static void activate_fault(const process_pfault &fault)
{
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
    fault.process->SetThreadFaulted(true);
#endif
    p_faults.push_back(fault);
}

void Process::activate_pfault_thread() {
    pfault_sema.release();
    if (pfault_thread != nullptr) {
        return;
    }
    std::lock_guard lock{pfault_thread_activate_single};
    if (pfault_thread == nullptr) {
        pfault_thread = new std::thread([] () {
            std::this_thread::set_name("[pfault]");
            while (true) {
                pfault_sema.acquire();
                process_pfault pfault{};
                {
                    std::lock_guard lock{pfault_lck};
                    auto iterator = p_faults.begin();
                    if (iterator == p_faults.end()) {
                        continue;
                    }
                    pfault = *iterator;
                    p_faults.erase(iterator);
                }
                if (pfault.pfaultTask != nullptr) {
                    pfault.process->GetProcess()->resolve_page_fault(*(pfault.process), *(pfault.pfaultTask), pfault.ip, pfault.address);
                } else {
                    pfault.process->GetProcess()->resolve_read_page(*(pfault.process), pfault.address, pfault.func);
                }
            }
        });
    }
}

bool Process::page_fault(ProcThread &thread, task &current_task, Interrupt &intr) {
#ifdef DEBUG_PAGE_FAULT_RESOLVE
    std::cout << "Page fault in user process with pagetable root " << std::hex << ((uintptr_t) get_root_pagetable())
    << " code " << intr.error_code() << std::dec << "\n";
#endif
    uint64_t address{0};
    asm("mov %%cr2, %0" : "=r"(address));
    if (sync_resolve_page(address)) {
        return true;
    }
    current_task.set_blocked(true);
    std::lock_guard lock{pfault_lck};
    process_pfault pf{.pfaultTask = &current_task, .process = &thread, .ip = intr.rip(), .address = address, .func = [] (bool) {}};
    activate_fault(pf);
    activate_pfault_thread();
    return true;
}

bool Process::exception(task &current_task, const std::string &name, Interrupt &intr) {
    std::cerr << "Exception <"<< name <<"> in user process at " << std::hex << intr.rip() << " code " << intr.error_code() << std::dec << "\n";
    {
        auto &cpu = intr.get_cpu_frame();
        auto &state = intr.get_cpu_state();
        std::cerr << std::hex << "ax=" << state.rax << " bx="<<state.rbx << " cx=" << state.rcx << " dx="<< state.rdx << "\n"
                  << "si=" << state.rsi << " di="<< state.rdi << " bp=" << state.rbp << " sp="<< cpu.rsp << "\n"
                  << "r8=" << state.r8 << " r9="<< state.r9 << " rA="<< state.r10 <<" rB=" << state.r11 << "\n"
                  << "rC=" << state.r12 << " rD="<< state.r13 <<" rE="<< state.r14 <<" rF="<< state.r15 << "\n"
                  << "cs=" << cpu.cs << " ds=" << state.ds << " es=" << state.es << " fs=" << state.fs << " gs="
                  << state.gs << " ss="<< cpu.ss <<"\n"
                  << "fsbase=" << state.fsbase << "\n" << std::dec;
    }
    current_task.set_end(true);
    return true;
}

uintptr_t Process::push_64(ProcThread &thread, uintptr_t ptr, uint64_t val, const std::function<void(bool, uintptr_t)> &func) {
    constexpr auto length = sizeof(val);
    uintptr_t dstptr{ptr - length};
    std::function<void (bool, uintptr_t)> f{func};
    resolve_read(thread, dstptr, length, true, [] (intptr_t) {}, [this, dstptr, val, length, f] (bool success, bool, const std::function<void (intptr_t)> &) mutable {
        if (!success) {
            f(false, 0);
            return resolve_return_value::AsyncReturn();
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
        vm.reload();
        void *ptr = (void *) ((uint8_t *) vm.pointer() + offset);
        *((uint64_t *) ptr) = val;
        f(true, dstptr);
        return resolve_return_value::AsyncReturn();
    });
    return dstptr;
}

uintptr_t Process::push_data(ProcThread &thread, uintptr_t ptr, const void *data, uintptr_t length, const std::function<void(bool, uintptr_t)> &func) {
    std::function<void (bool, uintptr_t)> f{func};
    if (length == 0) {
        f(true, ptr);
        return ptr;
    }
    uintptr_t dstptr{ptr - length};
    resolve_read(thread, dstptr, length, true, [] (intptr_t) {}, [this, dstptr, data, length, f] (bool success, bool, const std::function<void (intptr_t)> &) mutable {
        if (!success) {
            f(false, 0);
            return resolve_return_value::AsyncReturn();
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
        vm.reload();
        memcpy((uint8_t *) vm.pointer() + offset, data, length);
        f(true, dstptr);
        return resolve_return_value::AsyncReturn();
    });
    return dstptr;
}

uintptr_t Process::push_string(ProcThread &thread, uintptr_t ptr, const std::string &str, const std::function<void (bool, uintptr_t)> &func) {
    auto length = str.size() + 1;
    uintptr_t dstptr{ptr - length};
    std::string strcp{str};
    std::function<void (bool, uintptr_t)> f{func};
    resolve_read(thread, dstptr, length, true, [] (intptr_t) {}, [this, dstptr, strcp, length, f] (bool success, bool, const std::function<void (intptr_t)> &) mutable {
        if (!success) {
            f(false, 0);
            return resolve_return_value::AsyncReturn();
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
        vm.reload();
        memcpy((uint8_t *) vm.pointer() + offset, strcp.c_str(), length);
        f(true, dstptr);
        return resolve_return_value::AsyncReturn();
    });
    return dstptr;
}

void Process::push_strings(ProcThread &thread, uintptr_t ptr, const std::vector<std::string>::iterator &begin,
                           const std::vector<std::string>::iterator &end, const std::vector<uintptr_t> &strptrs,
                           const std::function<void(bool, const std::vector<uintptr_t> &, uintptr_t)> &func) {
    std::function<void(bool, const std::vector<uintptr_t> &, uintptr_t)> f{func};
    if (begin != end) {
        std::vector<std::string>::iterator b{begin};
        std::vector<std::string>::iterator e{end};
        std::vector<uintptr_t> strp{strptrs};
        push_string(thread, ptr, *b, [this, &thread, b, e, f, strp] (bool success, uintptr_t ptr) mutable {
            if (!success) {
                f(false, strp, ptr);
                return;
            }
            strp.push_back(ptr);
            ++b;
            push_strings(thread, ptr, b, e, strp, f);
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

bool Process::sync_resolve_page(uintptr_t fault_addr) {
    std::lock_guard<hw_spinlock> lock{mtx};
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
    if (!mapping->read) {
        std::cerr << "User process page resolve: No read mapping for page " << std::hex << page << std::dec << "\n";
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
#ifdef DEBUG_PAGE_FAULT_RESOLVE
        std::cout << "Loading page " << std::hex << page << " for addr " << fault_addr << " from " << physpage << std::dec << "\n";
#endif
        auto image = mapping->image;
        bool pre_existing{true};
        phys_t phys_page{0};
        filepage_ref loaded_page{};
        for (auto &mapped_page : mapping->mappings) {
            if (mapped_page.page == page) {
                phys_page = mapped_page.phys_page;
                loaded_page = mapped_page.data;
                break;
            }
        }
        if (phys_page == 0) {
            pre_existing = false;
            if (image) {
                return false;
            } else {
                phys_page = ppagealloc(PAGESIZE);
                if (phys_page != 0) {
                    vmem vm{PAGESIZE};
                    vm.page(0).rwmap(phys_page);
                    vm.reload();
                    bzero(vm.pointer(), PAGESIZE);
                }
            }
            if (phys_page == 0) {
                std::cerr << "User process page resolve: Failed to allocate phys page\n";
                return false;
            }
        }
        bool ok{false};
        for (auto &m : mappings) {
            if (m.pagenum <= page && page < (m.pagenum + m.pages)) {
                if (m.image) {
                    if (mapping != &m || physpage != (m.image_skip_pages + (page - m.pagenum)) || image != m.image) {
                        std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page
                                  << std::dec << "\n";
                        if (!loaded_page && !pre_existing) {
                            ppagefree(phys_page, PAGESIZE);
                        }
                        return false;
                    }
                } else {
                    if (mapping != &m || loaded_page) {
                        std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page
                                  << std::dec << "\n";
                        if (!loaded_page && !pre_existing) {
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
            if (!loaded_page && !pre_existing) {
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
                        if (!loaded_page && !pre_existing) {
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
                if (!loaded_page && !pre_existing) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            b2 = &((*(root->branches))[(page >> 9) & 511]);
#ifdef DEBUG_PAGE_FAULT_RESOLVE
            std::cout << "Low root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(*b2))) << std::dec << "\n";
#endif
            if (b2->present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page && !pre_existing) {
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
                        if (!loaded_page && !pre_existing) {
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
                if (!loaded_page && !pre_existing) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            auto &b1 = (*(root->branches))[(page >> (9+9)) & 511];
#ifdef DEBUG_PAGE_FAULT_RESOLVE
            std::cout << "Root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(b1))) << std::dec << "\n";
#endif
            if (b1.present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page && !pre_existing) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            vm.page(0).rmap(b1.page_ppn << 12);
            vm.reload();
            b2 = &((*((pagetable *) vm.pointer()))[(page >> 9) & 511]);
            if (b2->present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page && !pre_existing) {
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
#ifdef DEBUG_PAGE_FAULT_RESOLVE
            std::cout << std::hex << ((b2phys) + (page & 511)) << " *" << (b3.user_access != 0 ? "u" : "k") << (b3.writeable != 0 ? "w" : "r")
                      << (b3.execution_disabled != 0 ? "n" : "e") << " " << b3.page_ppn << std::dec
                      << "\n";
#endif
            if (!pre_existing) {
                mapping->mappings.push_back({.data = loaded_page, .phys_page = phys_page, .page = page});
            }
            b3.page_ppn = phys_page >> 12;
            b3.present = 1;
            b3.user_access = 1;
            if (b3.execution_disabled == 1 && !loaded_page && mapping->cow) {
                b3.writeable = 1;
            }
            reload_pagetables();
        }
        return true;
    } else {
        auto isCowMapping = mapping->cow;
        for (auto &pagemap : mapping->mappings) {
            if (page == pagemap.page) {
                if (pagemap.data) {
                    if (!isCowMapping) {
                        return false;
                    }
                    auto phys = ppagealloc(PAGESIZE);
                    if (phys == 0) {
                        std::cerr << "Error: Unable to allocate phys page for copy on write\n";
                        return false;
                    }
#ifdef DEBUG_PAGE_FAULT_RESOLVE
                    std::cout << "Memory copy " << std::hex << page << ": " << pagemap.data.PhysAddr() << " -> " << phys << "\n";
#endif
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
                } else if (pagemap.cow) {
                    auto phys = ppagealloc(PAGESIZE);
                    if (phys == 0) {
                        std::cerr << "Error: Unable to allocate phys page for copy on write\n";
                        return false;
                    }
#ifdef DEBUG_PAGE_FAULT_RESOLVE
                    std::cout << "Memory copy " << std::hex << page << ": " << pagemap.cow->GetPhysPage() << " -> " << phys << "\n";
#endif
                    vmem vm{PAGESIZE * 2};
                    vm.page(0).rwmap(phys);
                    vm.page(1).rmap(pagemap.cow->GetPhysPage());
                    vm.reload();
                    uint8_t *src = (uint8_t *) vm.pointer();
                    src += PAGESIZE;
                    memcpy(vm.pointer(), src, PAGESIZE);
                    b3.page_ppn = phys >> 12;
                    b3.writeable = 1;
                    vm.reload();
                    pagemap.cow = {};
                    pagemap.phys_page = phys;
                    return true;
                }
            }
        }
    }
    return false;
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
    if (!mapping->read) {
        std::cerr << "User process page resolve: No read mapping for page " << std::hex << page << std::dec << "\n";
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
#ifdef DEBUG_PAGE_FAULT_RESOLVE
        std::cout << "Loading page " << std::hex << page << " for addr " << fault_addr << " from " << physpage << std::dec << "\n";
#endif
        auto image = mapping->image;
        bool pre_existing{true};
        phys_t phys_page{0};
        filepage_ref loaded_page{};
        for (auto &mapped_page : mapping->mappings) {
            if (mapped_page.page == page) {
                phys_page = mapped_page.phys_page;
                loaded_page = mapped_page.data;
                break;
            }
        }
        if (phys_page == 0) {
            lock = {};
            pre_existing = false;
            if (image) {
                auto getPageResult = image->GetPage(physpage);
                if (getPageResult.status != kfile_status::SUCCESS) {
                    std::cerr << "Load page failed: " << text(getPageResult.status) << "\n";
                    return false;
                }
                loaded_page = getPageResult.result;
                phys_page = loaded_page.PhysAddr();
                if (mapping->load != 0) {
                    vmem vm{PAGESIZE * 2};
                    vm.page(0).rmap(phys_page);
                    phys_page = ppagealloc(PAGESIZE);
                    if (phys_page != 0) {
                        vm.page(1).rwmap(phys_page);
                        vm.reload();
                        memcpy((void *) ((uintptr_t) vm.pointer() + PAGESIZE), vm.pointer(), PAGESIZE);
                        bzero((void *) ((uintptr_t) vm.pointer() + PAGESIZE + mapping->load), PAGESIZE - mapping->load);
                        loaded_page = {};
                    } else {
                        loaded_page = {};
                    }
                }
            } else {
                std::cerr << "Should have been resolved synchronously\n";
                return false;
            }
            if (phys_page == 0) {
                std::cerr << "User process page resolve: Failed to allocate phys page\n";
                return false;
            }
            lock = std::make_unique<std::lock_guard<hw_spinlock>>(mtx);
        } else {
            std::cerr << "Should have been resolved synchronously\n";
            return false;
        }
        bool ok{false};
        for (auto &m : mappings) {
            if (m.pagenum <= page && page < (m.pagenum + m.pages)) {
                if (m.image) {
                    if (mapping != &m || physpage != (m.image_skip_pages + (page - m.pagenum)) || image != m.image) {
                        std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page
                                  << std::dec << "\n";
                        if (!loaded_page && !pre_existing) {
                            ppagefree(phys_page, PAGESIZE);
                        }
                        return false;
                    }
                } else {
                    if (mapping != &m || loaded_page) {
                        std::cerr << "User process page resolve: Mapping was removed for page " << std::hex << page
                                  << std::dec << "\n";
                        if (!loaded_page && !pre_existing) {
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
            if (!loaded_page && !pre_existing) {
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
                        if (!loaded_page && !pre_existing) {
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
                if (!loaded_page && !pre_existing) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            b2 = &((*(root->branches))[(page >> 9) & 511]);
#ifdef DEBUG_PAGE_FAULT_RESOLVE
            std::cout << "Low root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(*b2))) << std::dec << "\n";
#endif
            if (b2->present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page && !pre_existing) {
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
                        if (!loaded_page && !pre_existing) {
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
                if (!loaded_page && !pre_existing) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            auto &b1 = (*(root->branches))[(page >> (9+9)) & 511];
#ifdef DEBUG_PAGE_FAULT_RESOLVE
            std::cout << "Root branch " << std::hex << ((root->physpage << 12) + (((page >> (9+9)) & 511) * sizeof(b1))) << std::dec << "\n";
#endif
            if (b1.present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page && !pre_existing) {
                    ppagefree(phys_page, PAGESIZE);
                }
                return false;
            }
            vm.page(0).rmap(b1.page_ppn << 12);
            vm.reload();
            b2 = &((*((pagetable *) vm.pointer()))[(page >> 9) & 511]);
            if (b2->present == 0) {
                std::cerr << "User process page resolve: Pagetable directory present bit was cleared for page " << std::hex << page << std::dec << "\n";
                if (!loaded_page && !pre_existing) {
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
#ifdef DEBUG_PAGE_FAULT_RESOLVE
            std::cout << std::hex << ((b2phys) + (page & 511)) << " *" << (b3.user_access != 0 ? "u" : "k") << (b3.writeable != 0 ? "w" : "r")
                      << (b3.execution_disabled != 0 ? "n" : "e") << " " << b3.page_ppn << std::dec
                      << "\n";
#endif
            if (!pre_existing) {
                mapping->mappings.push_back({.data = loaded_page, .phys_page = phys_page, .page = page});
            }
            b3.page_ppn = phys_page >> 12;
            b3.present = 1;
            b3.user_access = 1;
            if (b3.execution_disabled == 1 && !loaded_page && mapping->cow) {
                b3.writeable = 1;
            }
            reload_pagetables();
        }
        return true;
    } else {
        std::cerr << "Should have been resolved synchronously\n";
        return false;
    }
}

ResolveWrite Process::resolve_write_page(uintptr_t fault_addr) {
    std::lock_guard lock{mtx};
    constexpr uint64_t highRelocationOffset = ((uint64_t) PMLT4_USERSPACE_HIGH_START) << (9 + 9 + 9 + 12);
    uint32_t page;
    if (fault_addr < highRelocationOffset) {
        constexpr phys_t lowUserpaceMax = ((phys_t) USERSPACE_LOW_END) << (9+9+12);
        if (fault_addr >= lowUserpaceMax) {
            std::cerr << "User process page resolve (w): Not within userpace memory limits\n";
            return ResolveWrite::ERROR;
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
        std::cerr << "User process page resolve (w): No mapping for page " << std::hex << page << std::dec << "\n";
        return ResolveWrite::ERROR;
    }
    if (!mapping->cow && mapping->image) {
        std::cerr << "User process page resolve (w): Read only page " << std::hex << page << std::dec << "\n";
        return ResolveWrite::ERROR;
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
            std::cerr << "User process page resolven (w): No low pagetable root for page " << std::hex << page << std::dec << "\n";
            return ResolveWrite::ERROR;
        }
        b2 = &((*(root->branches))[(page >> 9) & 511]);
        if (b2->present == 0) {
            std::cerr << "User process page resolve (w): Pagetable directory not present for page " << std::hex << page << std::dec << "\n";
            return ResolveWrite::ERROR;
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
            std::cerr << "User process page resolve (w): No pagetable root for page " << std::hex << page << std::dec << "\n";
            return ResolveWrite::ERROR;
        }
        auto b1 = (*(root->branches))[(page >> (9+9)) & 511];
        if (b1.present == 0) {
            std::cerr << "User process page resolve (w): Pagetable directory not present for page " << std::hex << page << std::dec << "\n";
            return ResolveWrite::ERROR;
        }
        vm.page(0).rmap(b1.page_ppn << 12);
        vm.reload();
        b2 = &((*((pagetable *) vm.pointer()))[(page >> 9) & 511]);
        if (b2->present == 0) {
            std::cerr << "User process page resolve (w): Pagetable not present for page " << std::hex << page << std::dec << "\n";
            return ResolveWrite::ERROR;
        }
    }
    vm.page(0).rwmap(b2->page_ppn << 12);
    vm.reload();
    auto &b3 = (*((pagetable *) vm.pointer()))[page & 511];
    if (b3.present == 0) {
        std::cerr << "User process page resolve (w): Page not present for page " << std::hex << page << std::dec << "\n";
        return ResolveWrite::ERROR;
    }
    if (b3.writeable != 0) {
        return ResolveWrite::WAS_WRITEABLE;
    }
    for (auto &pagemap : mapping->mappings) {
        if (page == pagemap.page) {
            if (pagemap.data) {
                if (!mapping->cow) {
                    break;
                }
                auto phys = ppagealloc(PAGESIZE);
                if (phys == 0) {
                    std::cerr << "Error: Unable to allocate phys page for copy on write\n";
                    return ResolveWrite::ERROR;
                }
#ifdef DEBUG_PAGE_FAULT_RESOLVE
                std::cout << "Memory copy " << std::hex << page << ": " << pagemap.data.PhysAddr() << " -> " << phys << "\n";
#endif
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
                return ResolveWrite::MADE_WRITEABLE;
            } else if (pagemap.cow) {
                auto phys = ppagealloc(PAGESIZE);
                if (phys == 0) {
                    std::cerr << "Error: Unable to allocate phys page for copy on write\n";
                    return ResolveWrite::ERROR;
                }
#ifdef DEBUG_PAGE_FAULT_RESOLVE
                std::cout << "Memory copy " << std::hex << page << ": " << pagemap.data.PhysAddr() << " -> " << phys << "\n";
#endif
                vmem vm{PAGESIZE * 2};
                vm.page(0).rwmap(phys);
                vm.page(1).rmap(pagemap.cow->GetPhysPage());
                vm.reload();
                uint8_t *src = (uint8_t *) vm.pointer();
                src += PAGESIZE;
                memcpy(vm.pointer(), src, PAGESIZE);
                b3.page_ppn = phys >> 12;
                b3.writeable = 1;
                vm.reload();
                pagemap.cow = {};
                pagemap.phys_page = phys;
                return ResolveWrite::MADE_WRITEABLE;
            }
        }
    }
    std::cerr << "User process page resolve (w): Page not mapped for write " << std::hex << page << std::dec << "\n";
    return ResolveWrite::ERROR;
}

void Process::resolve_page_fault(ProcThread &process, task &current_task, uintptr_t ip, uintptr_t fault_addr) {
    if (resolve_page(fault_addr)) {
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
        process.SetThreadFaulted(false);
#endif
        get_scheduler()->when_not_running(current_task, [&current_task] () {
            current_task.set_blocked(false);
        });
        return;
    }
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
    process.SetThreadFaulted(false);
#endif
    auto relocation = GetRelocationFor(ip);
    std::cerr << "PID " << current_task.get_id() << ": Page fault at " << std::hex << ip << " (" << relocation.filename
              << "+" << (ip-relocation.offset) << ") addr " << fault_addr << std::dec << "\n";
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

void Process::resolve_read_page(ProcThread &process, uintptr_t addr, std::function<void(bool)> func) {
    if (resolve_page(addr)) {
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
        process.SetThreadFaulted(false);
#endif
        func(true);
    } else {
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
        process.SetThreadFaulted(false);
#endif
        func(false);
    }
}

phys_t Process::phys_addr(uintptr_t addr) {
    uint64_t vpage{addr >> 12};
    uint32_t vpagerange{(uint32_t) vpage};
    if (vpage != vpagerange) {
        return 0;
    }
    auto pe = Pageentry(vpagerange);
    if (!pe || pe->present == 0) {
        return 0;
    }
    phys_t paddr{pe->page_ppn};
    paddr = paddr << 12;
    return paddr;
}

resolve_return_value Process::resolve_read_nullterm_impl(ProcThread &thread, uintptr_t addr, size_t item_size, size_t add_len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value(bool, bool, size_t, std::function<void (intptr_t)>)> func) {
    auto len = 0;
    while (readable(addr) || sync_resolve_page(addr)) {
        auto page = addr >> 12;
        auto pageaddr = page << 12;
        auto offset = addr - pageaddr;
        auto remaining = PAGESIZE - offset;
        auto ppageaddr = phys_addr(pageaddr);
        if (ppageaddr == 0) {
            auto result = func(false, async, 0, asyncReturn);
            if (async && !result.async) {
                asyncReturn(result.result);
                return resolve_return_value::AsyncReturn();
            }
            return result;
        }
        vmem vm{PAGESIZE};
        vm.page(0).rmap(ppageaddr);
        vm.reload();
        const char *str = (const char *) vm.pointer();
        str += offset;
        int i = 0;
        while (i < remaining) {
            bool nonzero{false};
            for (int j = 0; j < item_size; j++) {
                if (str[j] != 0) {
                    nonzero = true;
                    break;
                }
            }
            if (!nonzero) {
                auto result = func(true, async, len + add_len, asyncReturn);
                if (async && !result.async) {
                    asyncReturn(result.result);
                    return resolve_return_value::AsyncReturn();
                }
                return result;
            }
            str += item_size;
            i += item_size;
            ++len;
        }
        addr += i;
    }
    std::lock_guard lock{pfault_lck};
    activate_fault({.pfaultTask = nullptr, .process = &thread, .ip = 0, .address = addr, .func = [this, &thread, addr, func, item_size, len, add_len, asyncReturn] (bool success) mutable {
        if (!success) {
            auto result = func(false, true, 0, asyncReturn);
            if (!result.async) {
                asyncReturn(result.result);
            }
        } else {
            auto page = addr >> 12;
            auto pageaddr = page << 12;
            auto offset = addr - pageaddr;
            auto remaining = PAGESIZE - offset;
            auto paddr = phys_addr(pageaddr);
            if (paddr == 0) {
                auto result = func(false, true, 0, asyncReturn);
                if (!result.async) {
                    asyncReturn(result.result);
                }
                return;
            }
            vmem vm{PAGESIZE};
            vm.page(0).rmap(paddr);
            vm.reload();
            const char *str = (const char *) vm.pointer();
            str += offset;
            int i = 0;
            while (i < remaining) {
                bool nonzero{false};
                for (int j = 0; j < item_size; j++) {
                    if (str[j] != 0) {
                        nonzero = true;
                        break;
                    }
                }
                if (!nonzero) {
                    auto result = func(true, true, len + add_len, asyncReturn);
                    if (!result.async) {
                        asyncReturn(result.result);
                    }
                    return;
                }
                ++len;
                str += item_size;
                i += item_size;
            }
            addr += i;
            resolve_read_nullterm_impl(thread, addr, item_size, len + add_len, true, asyncReturn, func);
        }
    }});
    activate_pfault_thread();
    return resolve_return_value::AsyncReturn();
}

resolve_and_run Process::resolve_read_nullterm(ProcThread &thread, uintptr_t addr, size_t item_size, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value(bool, bool, size_t, std::function<void (intptr_t)>)> func) {
    auto result = resolve_read_nullterm_impl(thread, addr, item_size, 0, async, asyncReturn, func);
    return {.result = result.result, .async = result.async, .hasValue = result.hasValue};
}

resolve_and_run Process::resolve_read(ProcThread &thread, uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value(bool, bool, std::function<void (intptr_t)>)> func) {
    uintptr_t end = addr + len;
    if (addr == end) {
        auto result = func(true, async, asyncReturn);
        if (async && !result.async) {
            asyncReturn(result.result);
            return {.result = 0, .async = true, .hasValue = false};
        }
        return {.result = result.result, .async = result.async, .hasValue = result.hasValue};
    }
    while (readable(addr) || sync_resolve_page(addr)) {
        addr += 4096;
        len -= 4096;
        uintptr_t next_addr = addr & ~((uintptr_t) 4095);
        if (next_addr >= end) {
            auto result = func(true, async, asyncReturn);
            if (async && !result.async) {
                asyncReturn(result.result);
                return {.result = 0, .async = true, .hasValue = false};
            }
            return {.result = result.result, .async = result.async, .hasValue = result.hasValue};
        }
    }
    std::lock_guard lock{pfault_lck};
    activate_fault({.pfaultTask = nullptr, .process = &thread, .ip = 0, .address = addr, .func = [this, &thread, addr, len, end, func, asyncReturn] (bool success) mutable {
        if (!success) {
            auto result = func(false, true, asyncReturn);
            if (!result.async) {
                asyncReturn(result.result);
            }
        } else {
            addr += 4096;
            len -= 4096;
            uintptr_t next_addr = addr & ~((uintptr_t) 4095);
            if (next_addr < end) {
                auto result = resolve_read(thread, addr, len, true, asyncReturn, func);
                if (!result.async) {
                    asyncReturn(result.result);
                }
            } else {
                auto result = func(true, true, asyncReturn);
                if (!result.async) {
                    asyncReturn(result.result);
                }
            }
        }
    }});
    activate_pfault_thread();
    return {.result = 0, .async = true, .hasValue = false};
}

bool Process::resolve_write(uintptr_t addr, uintptr_t len) {
    uintptr_t end = addr + len;
    if (addr == end) {
        return true;
    }
    while (true) {
        auto res = resolve_write_page(addr);
        if (res != ResolveWrite::WAS_WRITEABLE && res != ResolveWrite::MADE_WRITEABLE) {
            return false;
        }
        addr += 4096;
        uintptr_t next_addr = addr & ~((uintptr_t) 4095);
        if (next_addr >= end) {
            return true;
        }
    }
}

kfile_result<std::shared_ptr<kfile>> Process::ResolveFile(const std::string &filename) {
    std::shared_ptr<kfile> cwd_ref{};
    {
        std::lock_guard lock{mtx};
        cwd_ref = this->cwd;
    }
    kdirectory *cwd = cwd_ref ? dynamic_cast<kdirectory *>(&(*cwd_ref)) : nullptr;

    std::shared_ptr<kfile> litem{};
    if (filename.starts_with("/")) {
        std::string resname{};
        resname.append(filename.c_str()+1);
        while (resname.starts_with("/")) {
            std::string trim{};
            trim.append(resname.c_str()+1);
            resname = trim;
        }
        if (!resname.empty()) {
            auto result = get_kernel_rootdir()->Resolve(resname);
            if (result.status != kfile_status::SUCCESS) {
                return {.result = {}, .status = result.status};
            }
            litem = result.result;
        } else {
            litem = get_kernel_rootdir();
        }
    } else {
        if (filename.empty() || cwd == nullptr) {
            return {};
        }
        auto result = cwd->Resolve(filename);
        if (result.status != kfile_status::SUCCESS) {
            return {.result = {}, .status = result.status};
        }
        litem = result.result;
    }
    return {.result = litem, .status = kfile_status::SUCCESS};
}

FileDescriptor Process::get_file_descriptor_impl(int fd) {
    for (auto desc : fileDescriptors) {
        if (desc.FD() == fd) {
            return desc;
        }
    }
    return {};
}

FileDescriptor Process::get_file_descriptor(int fd) {
    std::lock_guard lock{mtx};
    return get_file_descriptor_impl(fd);
}

FileDescriptor Process::create_file_descriptor(int openFlags, const std::shared_ptr<FileDescriptorHandler> &handler) {
    std::lock_guard lock{mtx};
    int fd = 0;
    while (get_file_descriptor_impl(fd).Valid()) { ++fd; }
    FileDescriptor desc{handler, fd, openFlags};
    fileDescriptors.push_back(desc);
    return desc;
}

FileDescriptor Process::create_file_descriptor(int openFlags, const std::shared_ptr<FileDescriptorHandler> &handler, int fd) {
    std::lock_guard lock{mtx};
    if (get_file_descriptor_impl(fd).Valid()) {
        return {};
    }
    FileDescriptor desc{handler, fd, openFlags};
    fileDescriptors.push_back(desc);
    return desc;
}

bool Process::close_file_descriptor(int fd) {
    std::lock_guard lock{mtx};
    auto iterator = fileDescriptors.begin();
    while (iterator != fileDescriptors.end()) {
        if (iterator->FD() == fd) {
            fileDescriptors.erase(iterator);
            return true;
        }
        ++iterator;
    }
    return false;
}

bool Process::brk(intptr_t brk_addr, uintptr_t &result) {
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
    if (program_brk > addr) {
        return false;
    }
    if ((addr - program_brk) <= PAGESIZE) {
        addr = program_brk;
    }
    auto original = addr;
    intptr_t delta_addr = brk_addr - ((intptr_t) addr);
    if (brk_addr <= 0) {
        delta_addr = brk_addr;
    }
    addr += delta_addr;
    if (addr > highEnd) {
        return false;
    }
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
        program_brk = result = addr;
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
    program_brk = result = addr;
    return true;
}

pid_t Process::getpgrp() {
    std::lock_guard lock{mtx};
    return pgrp;
}

int Process::setpgid(pid_t pid, pid_t pgid) {
    std::unique_lock lock{mtx};
    if (pid == 0 || pid == this->pid) {
        if (pgid == 0 || pgid == this->pid) {
            this->pgrp = this->pid;
            return 0;
        }
        lock.release();
        std::cerr << "setpgid: self("<<std::dec<<pid<<"), " << pgid << "\n";
        return -EINVAL;
    }
    std::cerr << "setpgid: "<<std::dec<<pid<<", " << pgid << "\n";
    return -EPERM;
}

int Process::sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize) {
    if (sigsetsize < 0 || sigsetsize > sizeof(sigmask)) {
        return -EINVAL;
    }
    if (how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK) {
        return -EINVAL;
    }
    std::lock_guard lock{mtx};
    if (oldset != nullptr) {
        memcpy(oldset, &sigmask, sigsetsize);
    }
    if (set != nullptr) {
        switch (how) {
            case SIG_BLOCK: {
                sigset_t tmp{};
                memcpy(&tmp, set, sigsetsize);
                for (size_t i = 0; i < (sizeof(tmp) / sizeof(tmp.val[0])); i++) {
                    sigmask.val[i] |= tmp.val[i];
                }
            }
                break;
            case SIG_UNBLOCK: {
                sigset_t tmp{};
                memcpy(&tmp, set, sigsetsize);
                for (size_t i = 0; i < (sizeof(tmp) / sizeof(tmp.val[0])); i++) {
                    sigmask.val[i] &= ~(tmp.val[i]);
                }
            }
                break;
            case SIG_SETMASK:
                memcpy(&sigmask, set, sigsetsize);
                break;
            default:
                return -EINVAL;
        }
    }
    return 0;
}

int Process::sigaction(int signal, const struct sigaction *act, struct sigaction *oact) {
    {
        std::lock_guard lock{mtx};
        for (auto &rec: sigactions) {
            if (rec.signal == signal) {
                if (oact != nullptr) {
                    memcpy(oact, &(rec.sigaction), sizeof(rec.sigaction));
                }
                if (act != nullptr) {
                    memcpy(&(rec.sigaction), act, sizeof(rec.sigaction));
                }
                return 0;
            }
        }
        if (act != nullptr) {
            sigactions.push_back({.signal = signal, .sigaction = *act});
        }
        if (oact) {
            bzero(oact, sizeof(*oact));
        }
    }
    return 0;
}

int Process::setrlimit(rlimit &lim, const rlimit &val) {
    if (val.rlim_max > lim.rlim_max || val.rlim_cur > lim.rlim_max) {
        return -EPERM;
    }
    if (val.rlim_max == rlim_INFINITY && lim.rlim_max != rlim_INFINITY) {
        return -EPERM;
    }
    if (val.rlim_cur == rlim_INFINITY && lim.rlim_max != rlim_INFINITY) {
        return -EPERM;
    }
    lim = val;
    return 0;
}

int Process::setrlimit(int resource, const rlimit &lim) {
    std::lock_guard lock{mtx};
    switch (resource) {
        case RLIMIT_CPU:
            return setrlimit(rlimits.Cpu, lim);
        case RLIMIT_FSIZE:
            return setrlimit(rlimits.FileSize, lim);
        case RLIMIT_DATA:
            return setrlimit(rlimits.Data, lim);
        case RLIMIT_STACK:
            return setrlimit(rlimits.Stack, lim);
        case RLIMIT_CORE:
            return setrlimit(rlimits.CoreSize, lim);
        case RLIMIT_RSS:
            return setrlimit(rlimits.Rss, lim);
        case RLIMIT_NPROC:
            return setrlimit(rlimits.Nproc, lim);
        case RLIMIT_NOFILE:
            return setrlimit(rlimits.Nofile, lim);
        case RLIMIT_MEMLOCK:
            return setrlimit(rlimits.Memlocks, lim);
        case RLIMIT_AS:
            return setrlimit(rlimits.AddressSpace, lim);
        case RLIMIT_LOCKS:
            return setrlimit(rlimits.Locks, lim);
        case RLIMIT_SIGPENDING:
            return setrlimit(rlimits.Sigpending, lim);
        case RLIMIT_MSGQUEUE:
            return setrlimit(rlimits.Msgqueue, lim);
        case RLIMIT_NICE:
            return setrlimit(rlimits.Nice, lim);
        case RLIMIT_RTPRIO:
            return setrlimit(rlimits.Rtprio, lim);
        case RLIMIT_RTTIME:
            return setrlimit(rlimits.Rttime, lim);
        default:
            return -EINVAL;
    }
    return -EINVAL;
}

int Process::getrlimit(int resource, rlimit &result) {
    std::lock_guard lock{mtx};
    switch (resource) {
        case RLIMIT_CPU:
            result = rlimits.Cpu;
            break;
        case RLIMIT_FSIZE:
            result = rlimits.FileSize;
            break;
        case RLIMIT_DATA:
            result = rlimits.Data;
            break;
        case RLIMIT_STACK:
            result = rlimits.Stack;
            break;
        case RLIMIT_CORE:
            result = rlimits.CoreSize;
            break;
        case RLIMIT_RSS:
            result = rlimits.Rss;
            break;
        case RLIMIT_NPROC:
            result = rlimits.Nproc;
            break;
        case RLIMIT_NOFILE:
            result = rlimits.Nofile;
            break;
        case RLIMIT_MEMLOCK:
            result = rlimits.Memlocks;
            break;
        case RLIMIT_AS:
            result = rlimits.AddressSpace;
            break;
        case RLIMIT_LOCKS:
            result = rlimits.Locks;
            break;
        case RLIMIT_SIGPENDING:
            result = rlimits.Sigpending;
            break;
        case RLIMIT_MSGQUEUE:
            result = rlimits.Msgqueue;
            break;
        case RLIMIT_NICE:
            result = rlimits.Nice;
            break;
        case RLIMIT_RTPRIO:
            result = rlimits.Rtprio;
            break;
        case RLIMIT_RTTIME:
            result = rlimits.Rttime;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

int Process::wake_all(uintptr_t addr) {
    std::vector<uint32_t> unblock{};
    {
        std::lock_guard lock{mtx};
        auto iterator = fwaits.begin();
        while (iterator != fwaits.end()) {
            auto &w = *iterator;
            if (w->addr == addr) {
                auto id = w->task_id;
                fwaits.erase(iterator);
                unblock.push_back(id);
                continue;
            }
            ++iterator;
        }
    }
    int count{0};
    auto scheduler = get_scheduler();
    scheduler->all_tasks([&unblock, &count] (task &t) {
        for (auto id : unblock) {
            if (id == t.get_id()) {
                t.set_blocked(false);
                ++count;
                return;
            }
        }
    });
    return count;
}

void Process::RegisterExitNotification(const std::function<void(intptr_t)> &func) {
    std::lock_guard lock{mtx};
    exitNotifications.push_back(func);
}

void Process::SetExitCode(intptr_t code) {
    std::lock_guard lock{mtx};
    exitCode = code;
}
