//
// Created by sigsegv on 7/16/22.
//

#include <iostream>
#include <errno.h>
#include <exec/procthread.h>
#include <core/scheduler.h>
#include <sys/mman.h>
#include "Mmap.h"

#define MMAP_DEBUG

int64_t Mmap::Call(int64_t addr, int64_t len, int64_t prot, int64_t flags, SyscallAdditionalParams &params) {
    int64_t fd = params.Param5();
    int64_t off = params.Param6();
#ifdef MMAP_DEBUG
    std::cout << "mmap(" << std::hex << addr << ", " << len << ", " << prot << ", " << flags << ", " << fd << ", " << off << std::dec << ")\n";
#endif
    {
        constexpr uint64_t nonSupportedProt = ~((uint64_t) PROT_SUPPORTED);
        constexpr uint64_t nonSupportedFlags = ~((uint64_t) MAP_SUPPORTED);

        auto protUnsupp = (uint64_t) prot & nonSupportedProt;
        auto flagsUnsupp = (uint64_t) flags & nonSupportedFlags;

        if (protUnsupp != 0 || flagsUnsupp != 0) {
            std::cerr << "mmap: error: unsupported prot/flags " << std::hex << prot << "/" << flags << std::dec << "\n";
            return -EOPNOTSUPP;
        }
    }

    auto mapType = flags & MAP_TYPE_MASK;

    switch (mapType) {
        case MAP_SHARED:
            std::cerr << "mmap: error: shared map is not impl\n";
            return -EOPNOTSUPP; // TODO
        case MAP_PRIVATE:
            break;
        default:
            return -EOPNOTSUPP;
    }

    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = current_task->get_resource<ProcThread>();

    if ((flags & MAP_FIXED) != 0) {
        std::cerr << "mmap: error: fixed map is not impl\n";
        return -EOPNOTSUPP; // TODO
    }

    if (prot != (PROT_READ | PROT_WRITE)) {
        std::cerr << "mmap: error: prot setting " << prot << " not implemented\n";
        return -EOPNOTSUPP;
    }

    auto pages = len >> 12;
    if ((pages << 12) != len) {
        pages++;
    }

    uintptr_t vpageaddr = process->FindFree(pages);
    if (vpageaddr == 0) {
        return -ENOMEM;
    }
    if (!process->Map(vpageaddr, pages, false)) {
        return -ENOMEM;
    }
    return vpageaddr << 12;
}
