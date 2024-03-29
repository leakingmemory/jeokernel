//
// Created by sigsegv on 8/9/22.
//

#include <exec/procthread.h>
#include <iostream>
#include <sys/mman.h>
#include <errno.h>
#include "Mprotect.h"

//#define MPROTECT_CALL_DEBUG

int64_t Mprotect::Call(int64_t addr, int64_t len , int64_t prot, int64_t, SyscallAdditionalParams &params) {
    constexpr uint64_t supportedProt = PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC;
    constexpr uint64_t notSupportedProt = ~supportedProt;
#ifdef MPROTECT_CALL_DEBUG
    std::cout << "mprotect(" << std::hex << addr << ", " << len << ", " << prot << std::dec << ")\n";
#endif
    if ((prot & notSupportedProt) != 0) {
        std::cerr << "mprotect: not supported prot flags 0x" << std::hex << prot << std::dec << "\n";
        return -EINVAL;
    }
    int rwx = prot & (PROT_READ | PROT_WRITE | PROT_EXEC);
    if (rwx != PROT_READ && rwx != PROT_NONE && rwx != (PROT_READ | PROT_WRITE)) {
        std::cerr << "mprotect: not supported prot rwx flags 0x" << std::hex << rwx << std::dec << "\n";
        return -EINVAL;
    }

    auto *process = params.CurrentThread();

    auto end = addr + len;
    {
        auto offset = end & (PAGESIZE - 1);
        if (offset > 0) {
            end += PAGESIZE - offset;
        }
    }
    end = end >> 12;
    auto page = addr >> 12;
    return process->Protect(page, end - page, rwx);
}