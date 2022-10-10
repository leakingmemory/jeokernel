//
// Created by sigsegv on 6/8/22.
//

#ifndef JEOKERNEL_PROCESS_H
#define JEOKERNEL_PROCESS_H

#include <memory>
#include <vector>
#include <mutex>
#include <signal.h>
#include <pagetable.h>
#include <core/vmem.h>
#include <pagealloc.h>
#include <core/scheduler.h>
#include <kfs/filepage_data.h>
#include <exec/fdesc.h>
#include <sys/resource.h>
#include <kfs/kfiles.h>
#include <tty/tty.h>
#include <exec/resolve_return.h>

class Process;

enum class ResolveWrite {
    ERROR, WAS_WRITEABLE, MADE_WRITEABLE
};

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
    uint16_t load;
    bool read;
    bool cow;
    bool binary_mapping;
    std::vector<PhysMapping> mappings;
public:
    MemMapping();
    static void CopyAttributes(MemMapping &, const MemMapping &);
    MemMapping(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool cow, bool binaryMapping);
    MemMapping(MemMapping &&mv);
    MemMapping(const MemMapping &) = delete;
    MemMapping &operator =(MemMapping &&mv);
    MemMapping &operator =(const MemMapping &) = delete;
    ~MemMapping();
};

constexpr rlim_t rlim_INFINITY = -1;

constexpr rlim_t rlim_AS = (64ULL*1024ULL*1024ULL*1024ULL);
constexpr rlim_t rlim_CORE = 1*1024*1024*1024;
constexpr rlim_t rlim_CPU = rlim_INFINITY;
constexpr rlim_t rlim_DATA = rlim_AS;
constexpr rlim_t rlim_FSIZE = rlim_INFINITY;
constexpr rlim_t rlim_LOCKS = 65536;
constexpr rlim_t rlim_MEMLOCK = (64ULL*1024ULL*1024ULL*1024ULL);
constexpr rlim_t rlim_MSGQUEUE = 32768;
constexpr rlim_t rlim_NICE = 40;
constexpr rlim_t rlim_NOFILE = 32768;
constexpr rlim_t rlim_RSS = 64ULL*1024ULL*1024ULL*1024ULL;
constexpr rlim_t rlim_RTPRIO = 99;
constexpr rlim_t rlim_RTTIME = 60*1000000;
constexpr rlim_t rlim_SIGPENDING = 512;
constexpr rlim_t rlim_STACK = 16*1024*1024;
constexpr rlim_t rlim_NPROC = 32768;

struct RLimits {
    rlimit AddressSpace{.rlim_cur = rlim_AS, .rlim_max = rlim_AS};
    rlimit CoreSize{.rlim_cur = rlim_CORE, .rlim_max = rlim_CORE};
    rlimit Cpu{.rlim_cur = rlim_CPU, .rlim_max = rlim_CPU};
    rlimit Data{.rlim_cur = rlim_DATA, .rlim_max = rlim_DATA};
    rlimit FileSize{.rlim_cur = rlim_FSIZE, .rlim_max = rlim_FSIZE};
    rlimit Locks{.rlim_cur = rlim_LOCKS, .rlim_max = rlim_LOCKS};
    rlimit Memlocks{.rlim_cur = rlim_MEMLOCK, .rlim_max = rlim_MEMLOCK};
    rlimit Msgqueue{.rlim_cur = rlim_MSGQUEUE, .rlim_max = rlim_MSGQUEUE};
    rlimit Nice{.rlim_cur = rlim_NICE, .rlim_max = rlim_NICE};
    rlimit Nofile{.rlim_cur = rlim_NOFILE, .rlim_max = rlim_NOFILE};
    rlimit Rss{.rlim_cur = rlim_RSS, .rlim_max = rlim_RSS};
    rlimit Rtprio{.rlim_cur = rlim_RTPRIO, .rlim_max = rlim_RTPRIO};
    rlimit Rttime{.rlim_cur = rlim_RTTIME, .rlim_max = rlim_RTTIME};
    rlimit Sigpending{.rlim_cur = rlim_SIGPENDING, .rlim_max = rlim_SIGPENDING};
    rlimit Stack{.rlim_cur = rlim_STACK, .rlim_max = rlim_STACK};
    rlimit Nproc{.rlim_cur = rlim_NPROC, .rlim_max = rlim_NPROC};
};

struct FutexWait {
    uintptr_t addr;
    uint32_t task_id;
};

struct BinaryRelocation {
    std::string filename;
    uintptr_t offset;

    BinaryRelocation() : filename(), offset(0) {}
    BinaryRelocation(const BinaryRelocation &cp) : filename(cp.filename), offset(cp.offset) {}
    BinaryRelocation(BinaryRelocation &&mv) : filename(std::move(mv.filename)), offset(mv.offset) {}
};

struct resolve_and_run {
    intptr_t result;
    bool async;
    bool hasValue;
};

struct sigaction_record {
    int signal;
    sigaction sigaction;
};

class ProcThread;

class Process {
private:
    hw_spinlock mtx;
    sigset_t sigmask;
    RLimits rlimits;
    pid_t pid;
    pid_t parent_pid;
    std::vector<PagetableRoot> pagetableLow;
    std::vector<PagetableRoot> pagetableRoots;
    std::vector<MemMapping> mappings;
    std::vector<FileDescriptor> fileDescriptors;
    std::vector<std::shared_ptr<FutexWait>> fwaits;
    std::vector<BinaryRelocation> relocations;
    std::shared_ptr<kfile> cwd;
    std::shared_ptr<tty> tty;
    std::vector<sigaction_record> sigactions;
    uintptr_t program_brk;
    int32_t euid, egid, uid, gid;
public:
    Process(const std::shared_ptr<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid);
    Process(const Process &) = delete;
    Process(Process &&) = delete;
    Process &operator =(const Process &) = delete;
    Process &operator =(Process &&) = delete;
private:
    std::optional<pageentr> Pageentry(uint32_t pagenum);
    bool Pageentry(uint32_t pagenum, std::function<void (pageentr &)> func);
    bool CheckMapOverlap(uint32_t pagenum, uint32_t pages);
    void activate_pfault_thread();
    bool readable(uintptr_t addr);
    bool resolve_page(uintptr_t fault_addr);
    void resolve_page_fault(ProcThread &process, task &current_task, uintptr_t ip, uintptr_t fault_addr);
    void resolve_read_page(ProcThread &process, uintptr_t addr, std::function<void (bool)> func);
    ResolveWrite resolve_write_page(uintptr_t addr);
public:
    phys_t phys_addr(uintptr_t addr);
private:
    resolve_return_value resolve_read_nullterm_impl(ProcThread &thread, uintptr_t addr, size_t add_len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func);
public:
    resolve_and_run resolve_read_nullterm(ProcThread &thread, uintptr_t addr, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func);
    resolve_and_run resolve_read(ProcThread &thread, uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func);
    bool resolve_write(uintptr_t addr, uintptr_t len);
    bool Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap);
    bool Map(uint32_t pagenum, uint32_t pages, bool binaryMap);
    int Protect(uint32_t pagenum, uint32_t pages, int prot);
    bool IsFree(uint32_t pagenum, uint32_t pages);
    bool IsInRange(uint32_t pagenum, uint32_t pages);
    void ClearRange(uint32_t pagenum, uint32_t pages);
    uint32_t FindFree(uint32_t pages);
    void SetProgramBreak(uintptr_t pbrk) {
        program_brk = pbrk;
    }
    uintptr_t GetProgramBreak() {
        return program_brk;
    }
    void AddRelocation(const std::string &filename, uintptr_t offset) {
        std::lock_guard lock{mtx};
        BinaryRelocation reloc{};
        reloc.filename = filename;
        reloc.offset = offset;
        relocations.push_back(reloc);
    }
    BinaryRelocation GetRelocationFor(uintptr_t ptr) {
        BinaryRelocation relocation{};
        relocation.filename = "<none>";
        std::lock_guard lock{mtx};
        for (const auto &cand : relocations) {
            if (cand.offset <= ptr && cand.offset >= relocation.offset) {
                relocation.filename = cand.filename;
                relocation.offset = cand.offset;
            }
        }
        return relocation;
    }
    void task_enter();
    void task_leave();
    bool page_fault(ProcThread &, task &current_task, Interrupt &intr);
    bool exception(task &current_task, const std::string &name, Interrupt &intr);
    uintptr_t push_data(ProcThread &, uintptr_t ptr, const void *, uintptr_t length, const std::function<void (bool,uintptr_t)> &);
private:
    uintptr_t push_string(ProcThread &, uintptr_t ptr, const std::string &, const std::function<void (bool,uintptr_t)> &);
public:
    void push_strings(ProcThread &, uintptr_t ptr, const std::vector<std::string>::iterator &, const std::vector<std::string>::iterator &, const std::vector<uintptr_t> &, const std::function<void (bool,const std::vector<uintptr_t> &,uintptr_t)> &);
    uintptr_t push_64(ProcThread &, uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &);
    kfile_result<std::shared_ptr<kfile>> ResolveFile(const std::string &filename);
private:
    FileDescriptor get_file_descriptor_impl(int);
public:
    FileDescriptor get_file_descriptor(int);
    FileDescriptor create_file_descriptor(const std::shared_ptr<FileDescriptorHandler> &handler);
    bool close_file_descriptor(int fd);
    std::shared_ptr<class tty> GetTty() const {
        return tty;
    }
    int32_t geteuid() const {
        return euid;
    }
    int32_t getegid() const {
        return egid;
    }
    int32_t getuid() const {
        return uid;
    }
    int32_t getgid() const {
        return gid;
    }
    bool brk(intptr_t delta_addr, uintptr_t &result);
    pid_t getpid() const {
        return pid;
    }
    pid_t getppid() const {
        return parent_pid;
    }
    std::shared_ptr<kfile> GetCwd() const {
        return cwd;
    }
    int sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
    int sigaction(int signal, const struct sigaction *act, struct sigaction *oact);
private:
    int setrlimit(rlimit &lim, const rlimit &val);
public:
    int setrlimit(int resource, const rlimit &lim);
    int getrlimit(int resource, rlimit &);
    int wake_all(uintptr_t addr);
};

#endif //JEOKERNEL_PROCESS_H
