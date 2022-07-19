//
// Created by sigsegv on 7/15/22.
//

#include <exec/procthread.h>

ProcThread::ProcThread() : process(new Process()), fsBase(0), tidAddress(0), robustListHead(0) {}

void ProcThread::resolve_read(uintptr_t addr, uintptr_t len, std::function<void(bool)> func) {
    process->resolve_read(addr, len, func);
}
bool ProcThread::Map(std::shared_ptr<kfile> image, uint32_t pagenum, uint32_t pages, uint32_t image_skip_pages, bool write, bool execute, bool copyOnWrite, bool binaryMap) {
    return process->Map(image, pagenum, pages, image_skip_pages, write, execute, copyOnWrite, binaryMap);
}
bool ProcThread::Map(uint32_t pagenum, uint32_t pages, bool binaryMap) {
    return process->Map(pagenum, pages, binaryMap);
}
uint32_t ProcThread::FindFree(uint32_t pages) {
    return process->FindFree(pages);
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
uintptr_t ProcThread::push_64(uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &func) {
    return process->push_64(ptr, val, func);
}
void ProcThread::push_strings(uintptr_t ptr, const std::vector<std::string> &strs, const std::function<void (bool,uintptr_t)> &func) {
    return process->push_strings(ptr, strs, func);
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
bool ProcThread::brk(intptr_t delta_addr, uintptr_t &result) {
    return process->brk(delta_addr, result);
}

pid_t ProcThread::getpid() {
    return process->getpid();
}
