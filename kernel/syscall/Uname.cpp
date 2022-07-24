//
// Created by sigsegv on 7/22/22.
//

#include <iostream>
#include <exec/procthread.h>
#include <errno.h>
#include "Uname.h"

#define UTSNAME_FIELD_LENGTH  65

struct utsname {
    char sysname[UTSNAME_FIELD_LENGTH];
    char nodename[UTSNAME_FIELD_LENGTH];
    char release[UTSNAME_FIELD_LENGTH];
    char version[UTSNAME_FIELD_LENGTH];
    char machine[UTSNAME_FIELD_LENGTH];
    char domainname[UTSNAME_FIELD_LENGTH];
};

constexpr size_t capLen(const std::string &str) {
    size_t strl = str.size();
    if (strl > (UTSNAME_FIELD_LENGTH-1)) {
        return UTSNAME_FIELD_LENGTH;
    }
    return strl + 1;
}

int64_t Uname::Call(int64_t utsnameptr, int64_t, int64_t, int64_t, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = current_task->get_resource<ProcThread>();
    current_task->set_blocked(true);
    process->resolve_read(utsnameptr, sizeof(utsname), [scheduler, process, current_task, utsnameptr] (bool success) {
        uint64_t pageaddr{(uint64_t) utsnameptr >> 12};
        uint64_t offset{(uint64_t) utsnameptr & (PAGESIZE-1)};
        int64_t result{0};
        if (!success) {
            result = -EFAULT;
            goto done_uname;
        }
        if (!process->resolve_write(utsnameptr, sizeof(utsname))) {
            result = -EFAULT;
            goto done_uname;
        }
        {
            vmem vm{offset + sizeof(utsname)};
            for (int i = 0; i < vm.npages(); i++) {
                uintptr_t vaddr{pageaddr + i};
                vaddr = vaddr << 12;
                auto phys = process->phys_addr(vaddr);
                if (phys == 0) {
                    std::cerr << "Unexpected: uname: no phys addr for vaddr "<<std::hex<<vaddr<<std::dec <<"\n";
                    result = -EFAULT;
                    goto done_uname;
                }
                vm.page(i).rwmap(phys);
            }
            vm.reload();
            utsname *utsn = (utsname *) (void *) (((uint8_t *) vm.pointer()) + offset);
            std::string sysname = "JEOKernel";
            std::string nodename = "local";
            std::string release = "0.1";
            std::string version = "0.1";
            std::string machine = "x86_64";
            std::string domainname = "dom";
            memset(utsn, 0, sizeof(*utsn));
            memcpy(utsn->sysname, sysname.data(), capLen(sysname));
            memcpy(utsn->nodename, sysname.data(), capLen(nodename));
            memcpy(utsn->release, sysname.data(), capLen(release));
            memcpy(utsn->version, sysname.data(), capLen(version));
            memcpy(utsn->machine, sysname.data(), capLen(machine));
            memcpy(utsn->domainname, sysname.data(), capLen(domainname));
            result = 0;
        }
done_uname:
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
    additionalParams.DoContextSwitch(true);
    return 0;
}