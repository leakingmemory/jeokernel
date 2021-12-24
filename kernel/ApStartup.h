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

class ApStartup {
private:
    cpu_mpfp *mpfp;
    LocalApic *lapic;
    IOApic *ioapic;
public:
    ApStartup(GlobalDescriptorTable *gdt, cpu_mpfp *mpfp, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its);
    ~ApStartup();
    void Init(PITTimerCalib *calib_timer);
    LocalApic *GetLocalApic() {
        return lapic;
    }
    IOApic *GetIoapic() {
        return ioapic;
    }
};

ApStartup *GetApStartup();


#endif //JEOKERNEL_APSTARTUP_H
