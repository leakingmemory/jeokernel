//
// Created by sigsegv on 7/15/22.
//

#ifndef JEOKERNEL_PROCTHREAD_H
#define JEOKERNEL_PROCTHREAD_H

#include <sys/types.h>
#include <exec/process.h>
#include <exec/rseq.h>
#include <exec/blockthread.h>

//#define DEBUG_SYSCALL_PFAULT_ASYNC_BUGS

struct sigaltstack {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
};

class ProcThread : public task_resource {
private:
    hw_spinlock mtx{};
    std::shared_ptr<Process> process;
    blockthread blockthr;
    ThreadRSeq rseq;
    struct sigaltstack sigaltstack;
    struct sigaltstack prevSigaltstack;
    std::function<void ()> aborterFunc{[] () {}};
    MemoryMapSnapshotBarrier threadRunMemMapSnapshot{};
    uintptr_t fsBase;
    uintptr_t tidAddress;
    uintptr_t robustListHead;
    pid_t tid;
    int syscallNumber;
    int aborterFuncHandle;
    bool clearTidAddr;
    bool hasSigaltstack;
    bool hasPrevSigaltstack;
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
    bool threadFaulted;
#endif
public:
    ProcThread(const reference<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid, const std::string &cmdline);
    explicit ProcThread(std::shared_ptr<Process> process);
    ~ProcThread();
    std::shared_ptr<Process> GetProcess() const {
        return process;
    }
    phys_t phys_addr(uintptr_t addr);
    resolve_and_run resolve_read_nullterm(uintptr_t addr, size_t item_size, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func);
    resolve_and_run resolve_read(uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func);
    bool resolve_write(uintptr_t addr, uintptr_t len);
    void QueueBlocking(uint32_t task_id, const std::function<void ()> &func) {
        blockthr.Queue(task_id, func);
    }
    void AborterFunc(const std::function<void ()> &func);
    void ClearAborterFunc();
    void CallAbort();
    void CallAbortAll();
    bool Map(const reference<kfile> &image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap);
    bool Map(uint32_t pagenum, uint32_t pages, bool binaryMap);
    int Protect(uint32_t pagenum, uint32_t pages, int prot);
    bool IsFree(uint32_t pagenum, uint32_t pages);
    bool IsInRange(uint32_t pagenum, uint32_t pages);
    void DisableRange(uint32_t pagenum, uint32_t pages);
    [[nodiscard]] std::shared_ptr<std::vector<DeferredReleasePage>> ClearRange(uint32_t pagenum, uint32_t pages);
    uint32_t FindFreeStart(uint32_t pages);
    uint32_t FindFree(uint32_t pages);
    std::shared_ptr<Process> Clone();
    void SetProgramBreak(uintptr_t pbrk);
    uintptr_t GetProgramBreak();
    void AddRelocation(const std::string &filename, uintptr_t offset);
    BinaryRelocation GetRelocationFor(uintptr_t ptr);
    void task_enter(task &, Interrupt *intr, uint8_t cpu) override;
    void task_leave() override;
    bool page_fault(task &current_task, Interrupt &intr) override;
    bool exception(task &current_task, const std::string &name, Interrupt &intr) override;
    uintptr_t push_data(uintptr_t ptr, const void *, uintptr_t length, const std::function<void (bool,uintptr_t)> &);
    uintptr_t push_64(uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &);
    void push_strings(uintptr_t ptr, const std::vector<std::string>::iterator &, const std::vector<std::string>::iterator &, const std::vector<uintptr_t> &, const std::function<void (bool,const std::vector<uintptr_t> &,uintptr_t)> &);
    kfile_result<reference<kfile>> ResolveFile(const std::shared_ptr<class referrer> &referrer, const std::string &filename);
    reference<FileDescriptor> get_file_descriptor(std::shared_ptr<class referrer> &referrer, int);
    bool has_file_descriptor(int);
    reference<FileDescriptor> create_file_descriptor(std::shared_ptr<class referrer> &referrer, int openFlags, const reference<FileDescriptorHandler> &handler);
    reference<FileDescriptor> create_file_descriptor(std::shared_ptr<class referrer> &referrer, int openFlags, const reference<FileDescriptorHandler> &handler, int fd);;
    bool close_file_descriptor(int fd);
    int32_t geteuid() const;
    int32_t getegid() const;
    int32_t getuid() const;
    int32_t getgid() const;
    bool brk(intptr_t delta_addr, uintptr_t &result);
    pid_t getpid() const;
    pid_t gettid() const;
    pid_t getpgrp() const;
    int setpgid(pid_t pid, pid_t pgid);
    int getpgid(pid_t pid);
    pid_t getppid() const;
    int sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
    int sigaction(int signal, const struct sigaction *act, struct sigaction *oact);
    std::optional<struct sigaction> GetSigaction(int signal);
    bool HasPendingSignalOrKill();
    int GetAndClearSigpending();
    void SetKilled();
    bool IsKilled() const;
    int setrlimit(int resource, const rlimit &lim);
    int getrlimit(int resource, rlimit &);
    reference<kfile> GetCwd(std::shared_ptr<class referrer> &referrer) const;
    ThreadRSeq &RSeq() {
        return this->rseq;
    }
    void SetFsBase(uintptr_t ptr) {
        fsBase = ptr;
    }
    void SetTidAddress(uintptr_t addr) {
        tidAddress = addr;
    }
    void SetClearTidAddress(bool clearTidAddress) {
        this->clearTidAddr = clearTidAddress;
    }
    void SetRobustListHead(uintptr_t addr) {
        robustListHead = addr;
    }
    void SetTid(pid_t tid) {
        this->tid = tid;
    }
    void SetSigaltstack(const struct sigaltstack &ss, struct sigaltstack &old_ss) {
        if (hasSigaltstack) {
            old_ss = sigaltstack;
        } else {
            old_ss = {};
        }
        prevSigaltstack = ss;
        hasPrevSigaltstack = hasSigaltstack;
        sigaltstack = ss;
        hasSigaltstack = true;
    }
    void SetSigaltstack(const struct sigaltstack &ss) {
        prevSigaltstack = ss;
        hasPrevSigaltstack = hasSigaltstack;
        sigaltstack = ss;
        hasSigaltstack = true;
    }
    void SetExitCode(intptr_t code);
    bool WaitForAnyChild(child_result &immediateResult, const std::function<void (pid_t, intptr_t)> &orAsync);
    void SetAuxv(const std::shared_ptr<const std::vector<ELF64_auxv>> &auxv);
    [[nodiscard]] std::shared_ptr<const std::vector<ELF64_auxv>> GetAuxv() const;
    void SetSyscallNumber(int sycallNumber);
    int GetSyscallNumber() const;

#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
    void SetThreadFaulted(bool faulted) {
        threadFaulted = faulted;
    }
    bool IsThreadFaulted() {
        return threadFaulted;
    }
#endif
};

#endif //JEOKERNEL_PROCTHREAD_H
