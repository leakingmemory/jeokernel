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
#include <resource/referrer.h>
#include <resource/reference.h>
#include "elf.h"

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
    bool twolevel;
public:
    PagetableRoot(uint16_t addr, bool twolevel);
    ~PagetableRoot();
    PagetableRoot(const PagetableRoot &cp) = delete;
    PagetableRoot & operator =(const PagetableRoot &) = delete;
    PagetableRoot & operator =(PagetableRoot &&) = delete;
    PagetableRoot(PagetableRoot &&mv);
};

class CowPageRef {
private:
    phys_t phys_page;
public:
    explicit CowPageRef(phys_t phys_page) : phys_page(phys_page) {};
    CowPageRef(const CowPageRef &) = delete;
    CowPageRef(CowPageRef &&) = delete;
    CowPageRef &operator =(const CowPageRef &) = delete;
    CowPageRef &operator =(CowPageRef &&) = delete;
    ~CowPageRef() {
        ppagefree(phys_page, PAGESIZE);
    }
    phys_t GetPhysPage() {
        return phys_page;
    }
};

struct PhysMapping {
    filepage_ref data;
    std::shared_ptr<CowPageRef> cow;
    phys_t phys_page;
    uint32_t page;
};

class MemMapping : public referrer {
    friend Process;
private:
    std::weak_ptr<MemMapping> selfRef{};
    reference<kfile> image{};
    uint32_t pagenum;
    uint32_t pages;
    uint32_t image_skip_pages;
    uint16_t load;
    bool read;
    bool cow;
    bool binary_mapping;
    std::vector<PhysMapping> mappings;
    MemMapping();
    MemMapping(uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool cow, bool binaryMapping);
public:
    static void CopyAttributes(MemMapping &, const MemMapping &);
    std::string GetReferrerIdentifier() override;
    void Init(const std::shared_ptr<MemMapping> &selfRef);
    void Init(const std::shared_ptr<MemMapping> &selfRef, const reference<kfile> &image);
    static std::shared_ptr<MemMapping> Create(const reference<kfile> &image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool cow, bool binaryMapping);
    static std::shared_ptr<MemMapping> Create();
    MemMapping(MemMapping &&mv) = delete;
    MemMapping(const MemMapping &) = delete;
    MemMapping &operator =(MemMapping &&mv) = delete;
    MemMapping &operator =(const MemMapping &) = delete;
    ~MemMapping();
};

class DeferredReleasePage {
private:
    uint32_t pagenum;
    filepage_ref data;
    std::shared_ptr<CowPageRef> cow;
public:
    DeferredReleasePage(uint32_t pagenum, filepage_ref data, std::shared_ptr<CowPageRef> cow)
        : pagenum(pagenum), data(data), cow(cow) {}
    DeferredReleasePage(const DeferredReleasePage &) = delete;
    DeferredReleasePage(DeferredReleasePage &&mv);
    DeferredReleasePage &operator =(const DeferredReleasePage &) = delete;
    ~DeferredReleasePage();
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
    struct sigaction sigaction;
};

struct child_result {
    intptr_t result;
    pid_t pid;
};

struct ProcessAborterFunc {
    std::function<void ()> func;
    int handle;
};

struct MemoryMapSnapshot {
    std::vector<std::shared_ptr<std::vector<DeferredReleasePage>>> deferredRelease;
    int serial;
    MemoryMapSnapshot(int serial) : deferredRelease(), serial(serial) {}
};

class MemoryMapSnapshotBarrier {
private:
    std::shared_ptr<Process> process;
    int serial;
    bool valid;
public:
    MemoryMapSnapshotBarrier(const std::shared_ptr<Process> &process);
    MemoryMapSnapshotBarrier() : process(), serial(0), valid(false) {}
    MemoryMapSnapshotBarrier(MemoryMapSnapshotBarrier &&mv) : process(mv.process), serial(mv.serial), valid(mv.valid) {
        mv.process = {};
        mv.valid = false;
    }
    MemoryMapSnapshotBarrier(const MemoryMapSnapshotBarrier &) = delete;
    MemoryMapSnapshotBarrier &operator =(MemoryMapSnapshotBarrier &&mv) noexcept {
        if (&mv != this) {
            Release();
            this->process = mv.process;
            this->serial = mv.serial;
            this->valid = mv.valid;
            mv.process = {};
            mv.valid = false;
        }
        return *this;
    }
    MemoryMapSnapshotBarrier &operator =(const MemoryMapSnapshotBarrier &) = delete;
    ~MemoryMapSnapshotBarrier() {
        Release();
    }
    void Release() noexcept;
};

class ProcThread;
struct MemoryArea;

struct process_pfault_thread {
    task *current_task;
    ProcThread *pthread;
    uintptr_t ip;
    uintptr_t fault_addr;
};

struct process_pfault_callback {
    ProcThread *pthread;
    std::function<void (bool)> func;
};

class Process : public referrer {
    friend MemoryMapSnapshotBarrier;
private:
    hw_spinlock mtx;
    std::weak_ptr<Process> self_ref;
    sigset_t sigmask;
    sigset_t sigpending;
    RLimits rlimits;
    pid_t pid;
    pid_t pgrp;
    std::weak_ptr<Process> parent;
    pid_t parent_pid;
    std::vector<PagetableRoot> pagetableLow;
    std::vector<PagetableRoot> pagetableRoots;
    std::vector<std::shared_ptr<MemMapping>> mappings;
    std::vector<MemoryMapSnapshot> memoryMapSnapshots{};
    std::vector<reference<FileDescriptor>> fileDescriptors;
    std::vector<BinaryRelocation> relocations;
    std::vector<ProcessAborterFunc> aborterFunc{};
    reference<kfile> cwd{};
    std::string cmdline;
    std::shared_ptr<class tty> tty;
    std::vector<sigaction_record> sigactions;
    std::vector<std::function<void (intptr_t)>> exitNotifications;
    std::vector<std::function<void (pid_t pid, intptr_t status)>> childExitNotifications;
    std::vector<child_result> childResults;
    std::shared_ptr<const std::vector<ELF64_auxv>> auxv;
    intptr_t exitCode;
    uintptr_t program_brk;
    int aborterFuncHandle{0};
    int memoryMapSerial{0};
    int32_t euid, egid, uid, gid;
    bool killed{false};
private:
    Process(const std::shared_ptr<class tty> &tty, pid_t parent_pid, const std::string &cmdline);
    Process(const Process &cp);
public:
    std::string GetReferrerIdentifier() override;
    static std::shared_ptr<Process> Create(const reference<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid, const std::string &cmdline);
    static std::shared_ptr<Process> Create(const std::shared_ptr<Process> &cp);
    Process(Process &&) = delete;
    Process &operator =(const Process &) = delete;
    Process &operator =(Process &&) = delete;
    ~Process();
    static pid_t GetMaxPid();
    static pid_t AllocTid();
private:
    void ChildExitNotification(pid_t pid, intptr_t status);
public:
    bool WaitForAnyChild(child_result &immediateResult, const std::function<void (pid_t, intptr_t)> &orAsync);
private:
    std::optional<pageentr> Pageentry(uint32_t pagenum);
    bool Pageentry(uint32_t pagenum, std::function<void (pageentr &)> func);
    bool CheckMapOverlap(uint32_t pagenum, uint32_t pages);
    void activate_pfault_thread();
    bool readable(uintptr_t addr);
    bool sync_resolve_page(uintptr_t fault_addr);
    bool resolve_page(uintptr_t fault_addr);
    void resolved_read_page(uintptr_t addr, const std::vector<process_pfault_thread> &, const std::vector<process_pfault_callback> &, bool success);
    ResolveWrite resolve_write_page(uintptr_t addr);
public:
    phys_t phys_addr(uintptr_t addr);
private:
    resolve_return_value resolve_read_nullterm_impl(ProcThread &thread, uintptr_t addr, size_t item_size, size_t add_len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func);
public:
    resolve_and_run resolve_read_nullterm(ProcThread &thread, uintptr_t addr, size_t item_size, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func);
private:
    resolve_and_run resolve_read(ProcThread *thread, uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func);
public:
    resolve_and_run resolve_read(ProcThread &thread, uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func) {
        return resolve_read(&thread, addr, len, async, asyncReturn, func);
    }
    resolve_and_run resolve_read(uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func) {
        return resolve_read(nullptr, addr, len, async, asyncReturn, func);
    }
    bool resolve_write(uintptr_t addr, uintptr_t len);
    bool Map(const reference<kfile> &image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap);
    bool Map(uint32_t pagenum, uint32_t pages, bool binaryMap);
    int Protect(uint32_t pagenum, uint32_t pages, int prot);
    bool IsFree(uint32_t pagenum, uint32_t pages);
    bool IsInRange(uint32_t pagenum, uint32_t pages);
    void DisableRange(uint32_t pagenum, uint32_t pages);
    [[nodiscard]] std::shared_ptr<std::vector<DeferredReleasePage>> ClearRange(uint32_t pagenum, uint32_t pages);
private:
    std::vector<MemoryArea> FindFreeMemoryAreas();
public:
    uint32_t FindFreeStart(uint32_t pages);
    uint32_t FindFree(uint32_t pages);
    void TearDownMemory();
private:
    std::vector<std::shared_ptr<MemMapping>> WriteProtectCow();
public:
    std::shared_ptr<Process> Clone();
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
    kfile_result<reference<kfile>> ResolveFile(const std::shared_ptr<class referrer> &referrer, const std::string &filename);
private:
    reference<FileDescriptor> get_file_descriptor_impl(const std::shared_ptr<class referrer> &, int);
    bool has_file_descriptor_impl(int);
public:
    reference<FileDescriptor> get_file_descriptor(const std::shared_ptr<class referrer> &, int);
    bool has_file_descriptor(int fd);
    reference<FileDescriptor> create_file_descriptor(const std::shared_ptr<class referrer> &, int openFlags, const reference<FileDescriptorHandler> &handler);
    reference<FileDescriptor> create_file_descriptor(const std::shared_ptr<class referrer> &, int openFlags, const reference<FileDescriptorHandler> &handler, int fd);
    bool close_file_descriptor(int fd);
    std::shared_ptr<class tty> GetTty() const {
        return tty;
    }
    void SetCmdLine(const std::string &cmdline) {
        this->cmdline = cmdline;
    }
    std::string GetCmdline() const {
        return cmdline;
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
    pid_t getpgrp();
    int setpgid(pid_t pid, pid_t pgid);
    int getpgid(pid_t pid);
    pid_t getppid() const {
        return parent_pid;
    }
    reference<kfile> GetCwd(std::shared_ptr<class referrer> &referrer) const;
    void SetCwd(const reference<kfile> &cwd);
    int sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
    int sigaction(int signal, const struct sigaction *act, struct sigaction *oact);
    std::optional<struct sigaction> GetSigaction(int signal);
    int setsignal(int signal);
    bool HasPendingSignalOrKill();
    int GetAndClearSigpending();
    void SetKilled();
    bool IsKilled();
    int AborterFunc(const std::function<void ()> &func);
    void ClearAborterFunc(int handle);
    void CallAbort();
    void CallAbortAll();
private:
    int setrlimit(rlimit &lim, const rlimit &val);
public:
    int setrlimit(int resource, const rlimit &lim);
    int getrlimit(int resource, rlimit &);
    void RegisterExitNotification(const std::function<void (intptr_t)> &func);
    void SetExitCode(intptr_t code);
    void SetAuxv(const std::shared_ptr<const std::vector<ELF64_auxv>> &auxv);
    [[nodiscard]] std::shared_ptr<const std::vector<ELF64_auxv>> GetAuxv() const;
};

#endif //JEOKERNEL_PROCESS_H
