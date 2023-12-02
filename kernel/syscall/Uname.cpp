//
// Created by sigsegv on 7/22/22.
//

#include <iostream>
#include <exec/procthread.h>
#include <core/uname_info.h>
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
    auto *scheduler = additionalParams.Scheduler();
    task *current_task = additionalParams.CurrentTask();
    auto *process = additionalParams.CurrentThread();
    auto result = process->resolve_read(utsnameptr, sizeof(utsname), false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    }, [process, utsnameptr] (bool success, bool, const std::function<void (intptr_t)> &) {
        uint64_t pageaddr{(uint64_t) utsnameptr >> 12};
        uint64_t offset{(uint64_t) utsnameptr & (PAGESIZE-1)};
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        if (!process->resolve_write(utsnameptr, sizeof(utsname))) {
            return resolve_return_value::Return(-EFAULT);
        }
        vmem vm{offset + sizeof(utsname)};
        for (int i = 0; i < vm.npages(); i++) {
            uintptr_t vaddr{pageaddr + i};
            vaddr = vaddr << 12;
            auto phys = process->phys_addr(vaddr);
            if (phys == 0) {
                std::cerr << "Unexpected: uname: no phys addr for vaddr "<<std::hex<<vaddr<<std::dec <<"\n";
                return resolve_return_value::Return(-EFAULT);
            }
            vm.page(i).rwmap(phys);
        }
        vm.reload();
        utsname *utsn = (utsname *) (void *) (((uint8_t *) vm.pointer()) + offset);
        std::string sysname = "Linux";
        std::string nodename = "local";
        std::string release{};
        std::string version = "0.1";
        std::string machine{};
        {
            uname_info info = get_uname_info();
            machine = info.arch;
            release.append(info.linux_level);
            release.append("-");
            release.append(info.variant);
            release.append("-");
            release.append(machine);
        }
        std::string domainname = "dom";
        memset(utsn, 0, sizeof(*utsn));
        memcpy(utsn->sysname, sysname.data(), capLen(sysname));
        memcpy(utsn->nodename, nodename.data(), capLen(nodename));
        memcpy(utsn->release, release.data(), capLen(release));
        memcpy(utsn->version, version.data(), capLen(version));
        memcpy(utsn->machine, machine.data(), capLen(machine));
        memcpy(utsn->domainname, domainname.data(), capLen(domainname));
        return resolve_return_value::Return(0);
    });
    if (result.async) {
        current_task->set_blocked(true);
        additionalParams.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}