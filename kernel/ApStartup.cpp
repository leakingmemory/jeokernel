//
// Created by sigsegv on 12/23/21.
//

#include "ApStartup.h"
#include <core/cpu_mpfp.h>
#include <core/LocalApic.h>
#include <thread>
#include "TaskStateSegment.h"
#include "InterruptTaskState.h"
#include "GlobalDescriptorTable.h"
#include "start_ap.h"
#include "PITTimerCalib.h"
#include "IOApic.h"

void set_tss(int cpun, struct TaskStateSegment *tss);
void set_its(int cpun, struct InterruptTaskState *its);

ApStartup::ApStartup(GlobalDescriptorTable *gdt, cpu_mpfp *mpfp, PITTimerCalib *calib_timer, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its) {
    LocalApic lapic{mpfp};
    IOApic ioApic{*mpfp};

    uint8_t vectors = ioApic.get_num_vectors();
    get_klogger() << "ioapic vectors " << vectors << "\n";
    for (uint8_t i = 0; i < vectors; i++) {
        ioApic.set_vector_interrupt(i, 0x23 + i);
    }

    int cpu_num = lapic.get_cpu_num(*mpfp);

    for (int i = 1; i <= mpfp->get_num_cpus(); i++) {
        get_klogger() << "Creating TSS ";
        TaskStateSegment *tss = (i - 1) != cpu_num ? new TaskStateSegment : cpu_tss;
        set_tss(i, tss);
        get_klogger() << (uint64_t) &(tss->get_entry()) << " its:";
        InterruptTaskState *int_task_state = (i - 1) != cpu_num ? new InterruptTaskState(*tss) : cpu_its;
        set_its(i, int_task_state);
        get_klogger() << (uint64_t) int_task_state << " for cpu " << (uint8_t) i << " to gdt sel ";
        get_klogger() << (uint16_t) tss->install(*gdt, i) << "\n";
    }

    gdt->reload();

    tasklist *scheduler = get_scheduler();
    scheduler->start_multi_cpu(cpu_num);

    const uint32_t *ap_count = install_ap_bootstrap();
    critical_section cli{};
    for (int i = 0; i < mpfp->get_num_cpus(); i++) {
        if (i != lapic.get_cpu_num(*mpfp)) {
            uint32_t apc = *ap_count;
            uint32_t exp_apc = apc + 1;
            uint8_t lapic_id = mpfp->get_cpu(i).local_apic_id;

            /*
             * AP needs the bootstrap stack, so enforce only one at a time
             */
            get_ap_start_lock()->lock();

            get_klogger() << "Starting cpu"<<((uint8_t) i)<<" "<<lapic_id<<"\n";
            lapic.send_init_ipi(lapic_id);
            calib_timer->delay(10000); //10ms
            lapic.send_sipi(lapic_id, 0x8);
            if (*ap_count == apc) {
                calib_timer->delay(1000); //1ms
                lapic.send_sipi(lapic_id, 0x8);
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