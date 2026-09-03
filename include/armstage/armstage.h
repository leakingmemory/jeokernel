//
// Created by sigsegv on 8/28/26.
//

#ifndef JEOKERNEL_ARMSTAGE_H
#define JEOKERNEL_ARMSTAGE_H

using u64 = __UINT64_TYPE__;

struct ArmStageContext {
    u64 ttbr;
    u64 kernel_entrypoint;
    u64 kernel_sp;
    u64 dtb;
    u64 uart;
    u64 phys_mem_base;
    u64 phys_mem_size;
    u64 root_pt;
    u64 cpu_id;
    u64 cpu_count;
};

#endif //JEOKERNEL_ARMSTAGE_H
