//
// Created by sigsegv on 12/23/21.
//

#ifndef JEOKERNEL_APSTARTUP_H
#define JEOKERNEL_APSTARTUP_H

class cpu_mpfp;
class GlobalDescriptorTable;
class TaskStateSegment;
class InterruptTaskState;
class PITTimerCalib;

class ApStartup {
public:
    ApStartup(GlobalDescriptorTable *gdt, cpu_mpfp *mpfp, PITTimerCalib *calib_timer, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its);
};


#endif //JEOKERNEL_APSTARTUP_H
