//
// Created by sigsegv on 12/23/21.
//

#ifndef JEOKERNEL_APSTARTUP_H
#define JEOKERNEL_APSTARTUP_H

#include <core/LocalApic.h>
#include "IOApic.h"

class cpu_mpfp;
class GlobalDescriptorTable;
class TaskStateSegment;
class InterruptTaskState;
class PITTimerCalib;
class acpi_madt_info;

class ApStartup {
private:
    cpu_mpfp *mpfp;
    std::shared_ptr<acpi_madt_info> madtptr;
    const apics_info *apicsInfo;
    LocalApic *lapic;
    IOApic *ioapic;
    int bsp_cpu_num;
public:
    ApStartup(GlobalDescriptorTable *gdt, cpu_mpfp *mpfp, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its);
    ~ApStartup();
    void Init(PITTimerCalib *calib_timer);
    int GetCpuNum() const;
    LocalApic *GetLocalApic() const {
        return lapic;
    }
    IOApic *GetIoapic() const {
        return ioapic;
    }
    bool IsBsp(int cpu_num) const {
        return cpu_num == bsp_cpu_num;
    }
};

ApStartup *GetApStartup();


#endif //JEOKERNEL_APSTARTUP_H
