//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_PROCESS_H
#define JEOKERNEL_PROCESS_H

#include <memory>
#include <vector>
#include <mutex>
#include <pagetable.h>
#include <core/vmem.h>
#include <pagealloc.h>
#include <core/scheduler.h>
#include <kfs/filepage_data.h>
#include <exec/fdesc.h>

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

struct PhysMapping {
    filepage_ref data;
    phys_t phys_page;
    uint32_t page;
};

class MemMapping {
    friend Process;
private:
    std::shared_ptr<kfile> image;
    uint32_t pagenum;
    uint32_t pages;
    uint32_t image_skip_pages;
    bool cow;
    std::vector<PhysMapping> mappings;
public:
    MemMapping(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool cow);
    ~MemMapping();
};

class Process : public task_resource {
private:
    hw_spinlock mtx;
    std::vector<PagetableRoot> pagetableLow;
    std::vector<PagetableRoot> pagetableRoots;
    std::vector<MemMapping> mappings;
    std::vector<FileDescriptor> fileDescriptors;
    int32_t euid, egid, uid, gid;
public:
    Process();
    Process(const Process &) = delete;
    Process(Process &&) = delete;
    Process &operator =(const Process &) = delete;
    Process &operator =(Process &&) = delete;
    std::optional<pageentr> Pageentry(uint32_t pagenum);
    bool Pageentry(uint32_t pagenum, std::function<void (pageentr &)> func);
    bool CheckMapOverlap(uint32_t pagenum, uint32_t pages);
    bool Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool write, bool execute, bool copyOnWrite);
    bool Map(uint32_t pagenum, uint32_t pages);
    uint32_t FindFree(uint32_t pages);
    void task_enter() override;
    void task_leave() override;
    void activate_pfault_thread();
    bool page_fault(task &current_task, Interrupt &intr) override;
    bool exception(task &current_task, const std::string &name, Interrupt &intr) override;
    uintptr_t push_64(uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &);
    uintptr_t push_string(uintptr_t ptr, const std::string &, const std::function<void (bool,uintptr_t)> &);
    void push_strings(uintptr_t ptr, const std::vector<std::string>::iterator &, const std::vector<std::string>::iterator &, const std::vector<uintptr_t> &, const std::function<void (bool,const std::vector<uintptr_t> &,uintptr_t)> &);
    void push_rev_ptrs(uintptr_t ptr, const std::vector<uintptr_t>::iterator &, const std::vector<uintptr_t>::iterator &, const std::function<void (bool,uintptr_t)> &);
    void push_strings(uintptr_t ptr, const std::vector<std::string> &, const std::function<void (bool,uintptr_t)> &);
    bool readable(uintptr_t addr);
    bool resolve_page(uintptr_t fault_addr);
    void resolve_page_fault(task &current_task, uintptr_t ip, uintptr_t fault_addr);
    void resolve_read_page(uintptr_t addr, std::function<void (bool)> func);
    void resolve_read(uintptr_t addr, uintptr_t len, std::function<void (bool)> func);
    FileDescriptor get_file_descriptor(int);
    int32_t geteuid() {
        return euid;
    }
    int32_t getegid() {
        return egid;
    }
    int32_t getuid() {
        return uid;
    }
    int32_t getgid() {
        return gid;
    }
};

#endif //JEOKERNEL_PROCESS_H
