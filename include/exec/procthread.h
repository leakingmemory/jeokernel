//
// Created by sigsegv on 7/15/22.
//

#ifndef JEOKERNEL_PROCTHREAD_H
#define JEOKERNEL_PROCTHREAD_H

#include <sys/types.h>
#include <exec/process.h>

class ProcThread : public task_resource {
private:
    std::shared_ptr<Process> process;
    uintptr_t fsBase;
    uintptr_t tidAddress;
    uintptr_t robustListHead;
public:
    ProcThread();
    void resolve_read(uintptr_t addr, uintptr_t len, std::function<void (bool)> func);
    bool Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool write, bool execute, bool copyOnWrite, bool binaryMap);
    bool Map(uint32_t pagenum, uint32_t pages, bool binaryMap);
    uint32_t FindFree(uint32_t pages);
    void task_enter() override;
    void task_leave() override;
    bool page_fault(task &current_task, Interrupt &intr) override;
    bool exception(task &current_task, const std::string &name, Interrupt &intr) override;
    uintptr_t push_data(uintptr_t ptr, const void *, uintptr_t length, const std::function<void (bool,uintptr_t)> &);
    uintptr_t push_64(uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &);
    void push_strings(uintptr_t ptr, const std::vector<std::string>::iterator &, const std::vector<std::string>::iterator &, const std::vector<uintptr_t> &, const std::function<void (bool,const std::vector<uintptr_t> &,uintptr_t)> &);
    FileDescriptor get_file_descriptor(int);
    int32_t geteuid();
    int32_t getegid();
    int32_t getuid();
    int32_t getgid();
    bool brk(intptr_t delta_addr, uintptr_t &result);
    pid_t getpid();
    void SetFsBase(uintptr_t ptr) {
        fsBase = ptr;
    }
    void SetTidAddress(uintptr_t addr) {
        tidAddress = addr;
    }
    void SetRobustListHead(uintptr_t addr) {
        robustListHead = addr;
    }
};

#endif //JEOKERNEL_PROCTHREAD_H
