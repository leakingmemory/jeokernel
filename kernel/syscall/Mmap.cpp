//
// Created by sigsegv on 7/16/22.
//

#include <iostream>
#include <errno.h>
#include <exec/procthread.h>
#include <core/scheduler.h>
#include <sys/mman.h>
#include "Mmap.h"

//#define MMAP_DEBUG

class Mmap_Call : public referrer {
private:
    std::weak_ptr<Mmap_Call> selfRef{};
    Mmap_Call() : referrer("Mmap_Call") {}
public:
    static std::shared_ptr<Mmap_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(uintptr_t addr, uintptr_t len, unsigned int prot, unsigned int flags, int fd, intptr_t off);
};

std::shared_ptr<Mmap_Call> Mmap_Call::Create() {
    std::shared_ptr<Mmap_Call> mmapCall{new Mmap_Call()};
    std::weak_ptr<Mmap_Call> weakPtr{mmapCall};
    mmapCall->selfRef = weakPtr;
    return mmapCall;
}

std::string Mmap_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t Mmap_Call::Call(uintptr_t addr, uintptr_t len, unsigned int prot, unsigned int flags, int fd, intptr_t off) {
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
    auto *process = scheduler->get_resource<ProcThread>(*current_task);

    auto pages = len >> 12;
    if ((pages << 12) != len) {
        pages++;
    }

    uintptr_t vpageaddr;
    std::shared_ptr<std::vector<DeferredReleasePage>> discardPages{};
    if ((flags & MAP_FIXED) == 0) {
        vpageaddr = process->FindFree(pages);
        if (vpageaddr == 0) {
            return -ENOMEM;
        }
    } else {
        vpageaddr = addr;
        if ((vpageaddr & (PAGESIZE-1)) != 0) {
            std::cerr << "mmap: address is not page aligned\n";
            return -EINVAL;
        }
        vpageaddr = vpageaddr >> 12;
        if (!process->IsInRange(vpageaddr, pages)) {
            std::cerr << "mmap: address is not in the allowed range for fixed mappings\n";
            return -ENOMEM;
        }
        discardPages = process->ClearRange(vpageaddr, pages);
        process->DisableRange(vpageaddr, pages);
    }
    if ((flags & MAP_ANONYMOUS) != 0) {
        if (!process->Map(vpageaddr, pages, false)) {
            return -ENOMEM;
        }
    } else {
        auto desc = process->get_file_descriptor(fd);
        if (!desc.Valid()) {
            return -EBADF;
        }
        std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
        auto file = desc.get_file(selfRef);
        if (!desc.can_read()) {
            return -EPERM;
        }
        if (!file || (flags & MAP_PRIVATE) == 0) {
            return -EOPNOTSUPP;
        }
        if ((off & (PAGESIZE-1)) != 0) {
            return -EINVAL;
        }
        uintptr_t file_offset = off;
        file_offset = file_offset >> 12;
        auto full_pages = len >> 12;
        if (!process->Map(file, vpageaddr, full_pages, file_offset, 0, (prot & PROT_WRITE) != 0, (prot & PROT_EXEC) != 0, (flags & MAP_PRIVATE) != 0, false)) {
            return -ENOMEM;
        }
        if (full_pages != pages) {
            file_offset += full_pages;
            auto load = len & (PAGESIZE - 1);
            if (!process->Map(file, vpageaddr + full_pages, 1, file_offset, load, (prot & PROT_WRITE) != 0, (prot & PROT_EXEC) != 0, (flags & MAP_PRIVATE) != 0, false)) {
                // TODO - unmap above
                return -ENOMEM;
            }
        }
    }
    return vpageaddr << 12;
}

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

    return Mmap_Call::Create()->Call(addr, len, prot, flags, fd, off);
}
