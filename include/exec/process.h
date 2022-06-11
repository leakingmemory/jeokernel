//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_PROCESS_H
#define JEOKERNEL_PROCESS_H

#include <memory>
#include <vector>
#include <pagetable.h>
#include <core/vmem.h>
#include <pagealloc.h>
#include <core/scheduler.h>

#define PAGESIZE 4096

class kfile;
class Process;

class PagetableRoot {
    friend Process;
private:
    uint64_t physpage;
    vmem vm;
    pagetable *branches;
    uint16_t addr;
public:
    PagetableRoot(uint16_t addr);
    ~PagetableRoot();
    PagetableRoot(const PagetableRoot &cp) = delete;
    PagetableRoot & operator =(const PagetableRoot &) = delete;
    PagetableRoot(PagetableRoot &&mv);
};

class MemMapping {
    friend Process;
private:
    std::shared_ptr<kfile> image;
    uint32_t pagenum;
    uint32_t pages;
    uint32_t image_skip_pages;
public:
    MemMapping(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages);
};

class Process : public task_resource {
private:
    std::vector<PagetableRoot> pagetableRoots;
    std::vector<MemMapping> mappings;
public:
    Process();
    bool Pageentry(uint32_t pagenum, std::function<void (pageentr &)> func);
    bool Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool write, bool execute, bool copyOnWrite);
    void task_enter() override;
    void task_leave() override;
    bool page_fault(task &current_task, Interrupt &intr) override;
};

#endif //JEOKERNEL_PROCESS_H
