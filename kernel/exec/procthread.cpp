//
// Created by sigsegv on 7/15/22.
//

#include <exec/procthread.h>

ProcThread::ProcThread() : process(new Process()), fsBase(0), tidAddress(0), robustListHead(0) {}

phys_t ProcThread::phys_addr(uintptr_t addr) {
    return process->phys_addr(addr);
}

void ProcThread::resolve_read_nullterm(uintptr_t addr, std::function<void(bool, size_t)> func) {
    process->resolve_read_nullterm(addr, func);
}

void ProcThread::resolve_read(uintptr_t addr, uintptr_t len, std::function<void(bool)> func) {
    process->resolve_read(addr, len, func);
}

bool ProcThread::resolve_write(uintptr_t addr, uintptr_t len) {
    return process->resolve_write(addr, len);
}
bool ProcThread::Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, uint16_t load, bool write, bool execute, bool copyOnWrite, bool binaryMap) {
    return process->Map(image, pagenum, pages, image_skip_pages, load, write, execute, copyOnWrite, binaryMap);
}
bool ProcThread::Map(uint32_t pagenum, uint32_t pages, bool binaryMap) {
    return process->Map(pagenum, pages, binaryMap);
}

int ProcThread::Protect(uint32_t pagenum, uint32_t pages, int prot) {
    return process->Protect(pagenum, pages, prot);
}
uint32_t ProcThread::FindFree(uint32_t pages) {
    return process->FindFree(pages);
}

void ProcThread::SetProgramBreak(uintptr_t pbrk) {
    process->SetProgramBreak(pbrk);
}
void ProcThread::task_enter() {
    process->task_enter();

    // MSR_FS_BASE -
    //asm("movl $0xC0000100, %%ecx; mov %0, %%rax; mov %%rax, %%rdx; shrq $32, %%rdx; wrmsr; " :: "r"(fsBase) : "%eax", "%rcx", "%rdx");
}
void ProcThread::task_leave() {
    process->task_leave();
}
bool ProcThread::page_fault(task &current_task, Interrupt &intr) {
    return process->page_fault(current_task, intr);
}
bool ProcThread::exception(task &current_task, const std::string &name, Interrupt &intr) {
    return process->exception(current_task, name, intr);
}

uintptr_t
ProcThread::push_data(uintptr_t ptr, const void *data, uintptr_t length, const std::function<void(bool, uintptr_t)> &func) {
    return process->push_data(ptr, data, length, func);
}
uintptr_t ProcThread::push_64(uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &func) {
    return process->push_64(ptr, val, func);
}

void ProcThread::push_strings(uintptr_t ptr, const std::vector<std::string>::iterator &begin,
                              const std::vector<std::string>::iterator &end, const std::vector<uintptr_t> &pointers,
                              const std::function<void(bool, const std::vector<uintptr_t> &, uintptr_t)> &func) {
    process->push_strings(ptr, begin, end, pointers, func);
}
FileDescriptor ProcThread::get_file_descriptor(int fd) {
    return process->get_file_descriptor(fd);
}
int32_t ProcThread::geteuid() {
    return process->geteuid();
}
int32_t ProcThread::getegid() {
    return process->getegid();
}
int32_t ProcThread::getuid() {
    return process->getuid();
}
int32_t ProcThread::getgid() {
    return process->getgid();
}
bool ProcThread::brk(intptr_t addr, uintptr_t &result) {
    return process->brk(addr, result);
}

pid_t ProcThread::getpid() {
    return process->getpid();
}

int ProcThread::sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize) {
    return process->sigprocmask(how, set, oldset, sigsetsize);
}

int ProcThread::setrlimit(int resource, const rlimit &lim) {
    return process->setrlimit(resource, lim);
}

int ProcThread::getrlimit(int resource, rlimit &lim) {
    return process->getrlimit(resource, lim);
}
