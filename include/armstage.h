#ifndef JEOKERNEL_ARMSTAGE_H
#define JEOKERNEL_ARMSTAGE_H

using u64 = __UINT64_TYPE__;

struct ArmStageContext {
    u64 ttbr;
    u64 kernel_entrypoint;
    u64 kernel_sp;
    u64 dtb;
};

#endif //JEOKERNEL_ARMSTAGE_H
