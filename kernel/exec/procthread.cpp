//
// Created by sigsegv on 7/15/22.
//

#include <exec/procthread.h>
#include <iostream>
#include <exec/usermem.h>
#include <exec/futex.h>

ProcThread::ProcThread(const std::shared_ptr<kfile> &cwd, const std::shared_ptr<class tty> &tty, pid_t parent_pid, const std::string &cmdline) :
process(Process::Create(cwd, tty, parent_pid, cmdline)), blockthr(), rseq(), fsBase(0), tidAddress(0), robustListHead(0), tid(process->getpid())
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
, threadFaulted(false)
#endif
{}

ProcThread::ProcThread(std::shared_ptr<Process> process) : process(process), blockthr(), rseq(), fsBase(0), tidAddress(0), robustListHead(0), tid(process->getpid())
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
, threadFaulted(false)
#endif
{

}

phys_t ProcThread::phys_addr(uintptr_t addr) {
    return process->phys_addr(addr);
}

resolve_and_run ProcThread::resolve_read_nullterm(uintptr_t addr, size_t item_size, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, size_t, std::function<void (intptr_t)>)> func) {
    return process->resolve_read_nullterm(*this, addr, item_size, async, asyncReturn, func);
}

resolve_and_run ProcThread::resolve_read(uintptr_t addr, uintptr_t len, bool async, std::function<void (intptr_t)> asyncReturn, std::function<resolve_return_value (bool, bool, std::function<void (intptr_t)>)> func) {
    return process->resolve_read(*this, addr, len, async, asyncReturn, func);
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

bool ProcThread::IsFree(uint32_t pagenum, uint32_t pages) {
    return process->IsFree(pagenum, pages);
}

uint32_t ProcThread::FindFreeStart(uint32_t pages) {
    return process->FindFreeStart(pages);
}

uint32_t ProcThread::FindFree(uint32_t pages) {
    return process->FindFree(pages);
}

std::shared_ptr<Process> ProcThread::Clone() {
    return process->Clone();
}

bool ProcThread::IsInRange(uint32_t pagenum, uint32_t pages) {
    return process->IsInRange(pagenum, pages);
}

void ProcThread::DisableRange(uint32_t pagenum, uint32_t pages) {
    process->DisableRange(pagenum, pages);
}

std::vector<DeferredReleasePage> ProcThread::ClearRange(uint32_t pagenum, uint32_t pages) {
    return process->ClearRange(pagenum, pages);
}

void ProcThread::SetProgramBreak(uintptr_t pbrk) {
    process->SetProgramBreak(pbrk);
}

uintptr_t ProcThread::GetProgramBreak() {
    return process->GetProgramBreak();
}

void ProcThread::AddRelocation(const std::string &filename, uintptr_t offset) {
    process->AddRelocation(filename, offset);
}

BinaryRelocation ProcThread::GetRelocationFor(uintptr_t ptr) {
    return process->GetRelocationFor(ptr);
}

void ProcThread::task_enter(task &t, Interrupt *intr, uint8_t cpu) {
    process->task_enter();

    rseq.Notify(*this, t, intr, cpu);

    // MSR_FS_BASE -
    //asm("movl $0xC0000100, %%ecx; mov %0, %%rax; mov %%rax, %%rdx; shrq $32, %%rdx; wrmsr; " :: "r"(fsBase) : "%eax", "%rcx", "%rdx");
}
void ProcThread::task_leave() {
    rseq.Preempted();

    process->task_leave();
}
bool ProcThread::page_fault(task &current_task, Interrupt &intr) {
    return process->page_fault(*this, current_task, intr);
}
bool ProcThread::exception(task &current_task, const std::string &name, Interrupt &intr) {
    return process->exception(current_task, name, intr);
}

uintptr_t
ProcThread::push_data(uintptr_t ptr, const void *data, uintptr_t length, const std::function<void(bool, uintptr_t)> &func) {
    return process->push_data(*this, ptr, data, length, func);
}
uintptr_t ProcThread::push_64(uintptr_t ptr, uint64_t val, const std::function<void (bool,uintptr_t)> &func) {
    return process->push_64(*this, ptr, val, func);
}

void ProcThread::push_strings(uintptr_t ptr, const std::vector<std::string>::iterator &begin,
                              const std::vector<std::string>::iterator &end, const std::vector<uintptr_t> &pointers,
                              const std::function<void(bool, const std::vector<uintptr_t> &, uintptr_t)> &func) {
    process->push_strings(*this, ptr, begin, end, pointers, func);
}

kfile_result<std::shared_ptr<kfile>> ProcThread::ResolveFile(const std::string &filename) {
    return process->ResolveFile(filename);
}

FileDescriptor ProcThread::get_file_descriptor(int fd) {
    return process->get_file_descriptor(fd);
}
FileDescriptor ProcThread::create_file_descriptor(int openFlags, const std::shared_ptr<FileDescriptorHandler> &handler) {
    return process->create_file_descriptor(openFlags, handler);
}
FileDescriptor ProcThread::create_file_descriptor(int openFlags, const std::shared_ptr<FileDescriptorHandler> &handler, int fd) {
    return process->create_file_descriptor(openFlags, handler, fd);
}
bool ProcThread::close_file_descriptor(int fd) {
    return process->close_file_descriptor(fd);
}

int32_t ProcThread::geteuid() const {
    return process->geteuid();
}
int32_t ProcThread::getegid() const {
    return process->getegid();
}
int32_t ProcThread::getuid() const {
    return process->getuid();
}
int32_t ProcThread::getgid() const {
    return process->getgid();
}
bool ProcThread::brk(intptr_t addr, uintptr_t &result) {
    return process->brk(addr, result);
}

pid_t ProcThread::getpid() const {
    return process->getpid();
}

pid_t ProcThread::gettid() const {
    return tid;
}

pid_t ProcThread::getpgrp() const {
    return process->getpgrp();
}

int ProcThread::setpgid(pid_t pid, pid_t pgid) {
    return process->setpgid(pid, pgid);
}

int ProcThread::getpgid(pid_t pid) {
    return process->getpgid(pid);
}

pid_t ProcThread::getppid() const {
    return process->getppid();
}

int ProcThread::sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize) {
    return process->sigprocmask(how, set, oldset, sigsetsize);
}

int ProcThread::sigaction(int signal, const struct sigaction *act, struct sigaction *oact) {
    return process->sigaction(signal, act, oact);
}

int ProcThread::setrlimit(int resource, const rlimit &lim) {
    return process->setrlimit(resource, lim);
}

int ProcThread::getrlimit(int resource, rlimit &lim) {
    return process->getrlimit(resource, lim);
}

std::shared_ptr<kfile> ProcThread::GetCwd() const {
    return process->GetCwd();
}

void ProcThread::SetExitCode(intptr_t code) {
    process->SetExitCode(code);
}

bool ProcThread::WaitForAnyChild(child_result &immediateResult, const std::function<void(pid_t, intptr_t)> &orAsync) {
    return process->WaitForAnyChild(immediateResult, orAsync);
}

void ProcThread::SetAuxv(const std::shared_ptr<const std::vector<ELF64_auxv>> &auxv) {
    process->SetAuxv(auxv);
}

std::shared_ptr<const std::vector<ELF64_auxv>> ProcThread::GetAuxv() const {
    return process->GetAuxv();
}
