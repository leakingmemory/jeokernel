#ifndef JEOKERNEL_ARMSTAGE_H
#define JEOKERNEL_ARMSTAGE_H

using u64 = __UINT64_TYPE__;

struct ArmStageContext {
    u64 ttbr;
    u64 kernel_entrypoint;
    u64 dtb;
    u64 uart;
    u64 phys_mem_base;
    u64 phys_mem_size;
    u64 root_pt;
    u64 cpu_count;
    u64 stage1data;
    u64 kernel_stacks[256];
};

#endif //JEOKERNEL_ARMSTAGE_H
