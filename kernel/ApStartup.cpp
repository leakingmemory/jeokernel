//
// Created by sigsegv on 12/23/21.
//

#include "ApStartup.h"
#include <core/cpu_mpfp.h>
#include <core/LocalApic.h>
#include <thread>
#include <acpi/acpi_madt_provider.h>
#include <acpi/acpi_madt.h>
#include "TaskStateSegment.h"
#include "InterruptTaskState.h"
#include "GlobalDescriptorTable.h"
#include "start_ap.h"
#include "PITTimerCalib.h"
#include "IOApic.h"

void set_tss(int cpun, struct TaskStateSegment *tss);
void set_its(int cpun, struct InterruptTaskState *its);

ApStartup::ApStartup(GlobalDescriptorTable *gdt, cpu_mpfp *mpfp, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its) {
    auto &acpi = get_acpi_madt_provider();
    madtptr = acpi.get_madt();
    if (!madtptr) {
        get_klogger() << "Can't find a madt\n";
    }
    apicsInfo = madtptr != nullptr ? madtptr->GetApicsInfo() : mpfp;

    lapic = new LocalApic(mpfp);
    ioapic = new IOApic(apicsInfo);

    uint8_t vectors = ioapic->get_num_vectors();
    get_klogger() << "ioapic vectors " << vectors << "\n";
    for (uint8_t i = 0; i < vectors; i++) {
        ioapic->set_vector_interrupt(i, 0x23 + i);
    }

    bsp_cpu_num = GetCpuNum();

    for (int i = 1; i <= apicsInfo->GetNumberOfCpus(); i++) {
        get_klogger() << "Creating TSS ";
        TaskStateSegment *tss = (i - 1) != bsp_cpu_num ? new TaskStateSegment : cpu_tss;
        set_tss(i, tss);
        get_klogger() << (uint64_t) &(tss->get_entry()) << " its:";
        InterruptTaskState *int_task_state = (i - 1) != bsp_cpu_num ? new InterruptTaskState(*tss) : cpu_its;
        set_its(i, int_task_state);
        get_klogger() << (uint64_t) int_task_state << " for cpu " << (uint8_t) i << " to gdt sel ";
        get_klogger() << (uint16_t) tss->install(*gdt, i) << "\n";
    }

    gdt->reload();

}

ApStartup::~ApStartup() {
    delete ioapic;
    delete lapic;
    ioapic = nullptr;
    lapic = nullptr;
}

void ApStartup::Init(PITTimerCalib *calib_timer) {
    tasklist *scheduler = get_scheduler();
    scheduler->start_multi_cpu(bsp_cpu_num);

    const uint32_t *ap_count = install_ap_bootstrap();

    critical_section cli{};
    for (int i = 0; i < apicsInfo->GetNumberOfCpus(); i++) {
        if (i != bsp_cpu_num) {
            uint32_t apc = *ap_count;
            uint32_t exp_apc = apc + 1;
            uint8_t lapic_id = apicsInfo->GetLocalApicId(i);

            /*
             * AP needs the bootstrap stack, so enforce only one at a time
             */
            get_ap_start_lock()->lock();

            get_klogger() << "Starting cpu"<<((uint8_t) i)<<" "<<lapic_id<<"\n";
            lapic->send_init_ipi(lapic_id);
            calib_timer->delay(10000); //10ms
            lapic->send_sipi(lapic_id, 0x8);
            if (*ap_count == apc) {
                calib_timer->delay(1000); //1ms
                lapic->send_sipi(lapic_id, 0x8);
                calib_timer->delay(10000); //10ms
            }
            if (*ap_count == exp_apc) {
                get_klogger() << "Started cpu"<<((uint8_t) i)<<" "<<lapic_id<<"\n";
            } else {
                if (*ap_count != apc) {
                    wild_panic("Something went wrong with starting AP(CPU)");
                }
                get_klogger() << "Failed to start cpu"<<((uint8_t) i)<<" "<<lapic_id<<"\n";
            }
        }
    }
}

int ApStartup::GetCpuNum() const {
    return lapic->get_cpu_num(apicsInfo);
}
