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

class ProcThread : public task_resource {
private:
    std::shared_ptr<Process> process;
    blockthread blockthr;
    ThreadRSeq rseq;
    uintptr_t fsBase;
    uintptr_t tidAddress;
    uintptr_t robustListHead;
    pid_t tid;
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
    bool threadFaulted;
#endif
public:
    ProcThread(const std::shared_ptr<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid, const std::string &cmdline);
    explicit ProcThread(std::shared_ptr<Process> process);
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
    bool Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap);
    bool Map(uint32_t pagenum, uint32_t pages, bool binaryMap);
    int Protect(uint32_t pagenum, uint32_t pages, int prot);
    bool IsFree(uint32_t pagenum, uint32_t pages);
    bool IsInRange(uint32_t pagenum, uint32_t pages);
    void DisableRange(uint32_t pagenum, uint32_t pages);
    [[nodiscard]] std::vector<DeferredReleasePage> ClearRange(uint32_t pagenum, uint32_t pages);
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
    kfile_result<std::shared_ptr<kfile>> ResolveFile(const std::string &filename);
    FileDescriptor get_file_descriptor(int);
    FileDescriptor create_file_descriptor(int openFlags, const std::shared_ptr<FileDescriptorHandler> &handler);
    FileDescriptor create_file_descriptor(int openFlags, const std::shared_ptr<FileDescriptorHandler> &handler, int fd);;
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
    int setrlimit(int resource, const rlimit &lim);
    int getrlimit(int resource, rlimit &);
    int wake_all(uintptr_t addr);
    std::shared_ptr<kfile> GetCwd() const;
    ThreadRSeq &RSeq() {
        return this->rseq;
    }
    void SetFsBase(uintptr_t ptr) {
        fsBase = ptr;
    }
    void SetTidAddress(uintptr_t addr) {
        tidAddress = addr;
    }
    void SetRobustListHead(uintptr_t addr) {
        robustListHead = addr;
    }
    void SetTid(pid_t tid) {
        this->tid = tid;
    }
    void SetExitCode(intptr_t code);
    bool WaitForAnyChild(child_result &immediateResult, const std::function<void (pid_t, intptr_t)> &orAsync);
    void SetAuxv(const std::shared_ptr<const std::vector<ELF64_auxv>> &auxv);
    [[nodiscard]] std::shared_ptr<const std::vector<ELF64_auxv>> GetAuxv() const;

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
