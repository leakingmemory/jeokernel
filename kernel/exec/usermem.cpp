//
// Created by sigsegv on 7/27/22.
//

#include <core/vmem.h>
#include <exec/usermem.h>
#include <exec/procthread.h>

UserMemory::UserMemory(Process &proc, uintptr_t uptr, uintptr_t len, bool write) : vm(), valid(false) {
    if (uptr == 0) {
        return;
    }
    offset = uptr & (PAGESIZE-1);
    auto base = uptr - offset;
    vm = std::make_shared<vmem>(len + offset);
    for (int i = 0; i < vm->npages(); i++) {
        auto phys = proc.phys_addr(base);
        if (phys == 0) {
            vm = {};
            return;
        }
        if (write) {
            vm->page(i).rwmap(phys);
        } else {
            vm->page(i).rmap(phys);
        }
        base += PAGESIZE;
    }
    vm->reload();
    valid = true;
}

UserMemory::UserMemory(ProcThread &proc, uintptr_t uptr, uintptr_t len, bool write) : UserMemory(*(proc.GetProcess()), uptr, len, write) {}

void *UserMemory::Pointer() const {
    return (void *) ((uintptr_t) vm->pointer() + offset);
}
