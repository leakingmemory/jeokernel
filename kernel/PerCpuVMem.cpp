//
// Created by sigsegv on 6/16/22.
//

#include <klogger.h>
#include "PerCpuVMem.h"

static PerCpuVMem *perCpuVMem = nullptr;

PerCpuVMem::PerCpuVMem() : pagetables(), freePages() {
    pagetables = vpercpuallocpagetable32();
    if (pagetables.pointer == 0) {
        wild_panic("Couldn't alloc vpercpu");
    }
    for (int i = 0; i < vpagetablecount(); i++) {
        freePages.push_back(i);
    }
}

PerCpuVMem &PerCpuVMem::Instance() {
    if (perCpuVMem == nullptr) {
        perCpuVMem = new PerCpuVMem;
    }
    return *perCpuVMem;
}

uintptr_t PerCpuVMem::AllocStack() {
    constexpr uint16_t needPages = 8;
    uint16_t start{1};
    uint16_t end{0};
    for (auto freePage : freePages) {
        if ((end + 1) == freePage) {
            end = freePage;
            if ((end - start + 1) >= needPages) {
                phys_t paddr = ppagealloc((needPages - 1) * pagetables.numTables * 4096);
                if (paddr == 0) {
                    return 0;
                }
                for (int i = 0; i < pagetables.numTables; i++) {
                    {
                        auto &pe = pagetables.tables[i][0];
                        pe.page_ppn = 0;
                        pe.present = 0;
                    }
                    for (int j = 1; j < needPages; j++) {
                        auto &pe = pagetables.tables[i][j];
                        pe.page_ppn = (paddr >> 12) + (i * (needPages - 1)) + j - 1;
                        pe.present = 1;
                        pe.execution_disabled = 1;
                        pe.writeable = 1;
                        pe.user_access = 0;
                        pe.accessed = 0;
                        pe.dirty = 0;
                    }
                }
                auto iterator = freePages.begin();
                while (iterator != freePages.end()) {
                    auto p = *iterator;
                    if (p >= start && p <= end) {
                        freePages.erase(iterator);
                    } else {
                        ++iterator;
                    }
                }
                ++end;
                return pagetables.pointer + (end << 12);
            }
        } else {
            start = freePage;
            end = freePage;
        }
    }
    return 0;
}