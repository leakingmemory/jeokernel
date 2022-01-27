//
// Created by sigsegv on 12/23/21.
//

#ifndef JEOKERNEL_APSTARTUP_H
#define JEOKERNEL_APSTARTUP_H

#include <mutex>
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
    std::mutex mtx;
    std::vector<std::tuple<uint64_t,uint64_t>> reserved_mem;
    std::shared_ptr<acpi_madt_info> madtptr;
    std::unique_ptr<cpu_mpfp> mpfp;
    const apics_info *apicsInfo;
    LocalApic *lapic;
    IOApic *ioapic;
    int bsp_cpu_num;
public:
    ApStartup(GlobalDescriptorTable *gdt, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its, const std::vector<std::tuple<uint64_t,uint64_t>> &reserved_mem);
    ~ApStartup();
    void Init(PITTimerCalib *calib_timer);
    int GetCpuNum() const;
    LocalApic *GetLocalApic() const {
        return lapic;
    }
    IOApic *GetIoapic() const {
        return ioapic;
    }

    uint16_t GetIsaIoapicPin(uint8_t isaIrq);

    cpu_mpfp *GetMpTable();

    bool IsBsp(int cpu_num) const {
        return cpu_num == bsp_cpu_num;
    }
};

ApStartup *GetApStartup();


#endif //JEOKERNEL_APSTARTUP_H
