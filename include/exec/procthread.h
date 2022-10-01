//
// Created by sigsegv on 7/15/22.
//

#ifndef JEOKERNEL_PROCTHREAD_H
#define JEOKERNEL_PROCTHREAD_H

#include <sys/types.h>
#include <exec/process.h>
#include <exec/rseq.h>

#define DEBUG_SYSCALL_PFAULT_ASYNC_BUGS

class ProcThread : public task_resource {
private:
    std::shared_ptr<Process> process;
    ThreadRSeq rseq;
    uintptr_t fsBase;
    uintptr_t tidAddress;
    uintptr_t robustListHead;
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
    bool threadFaulted;
#endif
public:
    ProcThread(const std::shared_ptr<kfile> &cwd);
    std::shared_ptr<Process> GetProcess() {
        return process;
    }
    phys_t phys_addr(uintptr_t addr);
    resolve_and_run resolve_read_nullterm(uintptr_t addr, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func);
    resolve_and_run resolve_read(uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func);
    bool resolve_write(uintptr_t addr, uintptr_t len);
    bool Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap);
    bool Map(uint32_t pagenum, uint32_t pages, bool binaryMap);
    int Protect(uint32_t pagenum, uint32_t pages, int prot);
    bool IsFree(uint32_t pagenum, uint32_t pages);
    bool IsInRange(uint32_t pagenum, uint32_t pages);
    void ClearRange(uint32_t pagenum, uint32_t pages);;
    uint32_t FindFree(uint32_t pages);
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
    FileDescriptor create_file_descriptor(const std::shared_ptr<FileDescriptorHandler> &handler);
    bool close_file_descriptor(int fd);
    int32_t geteuid();
    int32_t getegid();
    int32_t getuid();
    int32_t getgid();
    bool brk(intptr_t delta_addr, uintptr_t &result);
    pid_t getpid();
    int sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
    int setrlimit(int resource, const rlimit &lim);
    int getrlimit(int resource, rlimit &);
    int wake_all(uintptr_t addr);
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
