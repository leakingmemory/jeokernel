//
// Created by sigsegv on 4/19/26.
//

#ifndef JEOKERNEL_UEFISTAGE_H
#define JEOKERNEL_UEFISTAGE_H

struct UefiStageContext {
    uint64_t vmem_root; // cr3
    uint64_t physpage_map;
    uint64_t entrypoint_addr;
    uint64_t pml4t_addr;
};

struct UefiStageInfo {
};

#endif //JEOKERNEL_UEFISTAGE_H
