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
#include "SyscallSupport.h"
#include <pagealloc.h>

void set_tss(int cpun, struct TaskStateSegment *tss);
void set_its(int cpun, struct InterruptTaskState *its);

ApStartup::ApStartup(GlobalDescriptorTable *gdt, TaskStateSegment *cpu_tss, InterruptTaskState *cpu_its, const std::vector<std::tuple<uint64_t,uint64_t>> &reserved_mem) :
        mtx(), reserved_mem(reserved_mem) {
    auto &acpi = get_acpi_madt_provider();
    madtptr = acpi.get_madt();
    if (!madtptr) {
        get_klogger() << "Can't find a madt\n";
    }
    if (madtptr != nullptr) {
        apicsInfo = madtptr->GetApicsInfo();
    } else {
        mpfp = std::unique_ptr<cpu_mpfp>(new cpu_mpfp(reserved_mem));
        if (mpfp->is_valid()) {
            apicsInfo = &(*mpfp);
        } else {
            mpfp = {};
            apicsInfo = nullptr;
        }
    }

    lapic = new LocalApic(nullptr);
    ioapic = new IOApic(apicsInfo);

    uint16_t vectors = ioapic->get_num_vectors();
    get_klogger() << "ioapic vectors " << vectors << "\n";
    for (uint16_t i = 0; i < vectors; i++) {
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
        get_klogger() << (uint16_t) tss->install(*gdt, i - 1) << "\n";
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

    vmem_switch_to_multicpu(this, apicsInfo->GetNumberOfCpus());
    SyscallSupport::Instance().GlobalSetup().CpuSetup();

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

cpu_mpfp *ApStartup::GetMpTable() {
    std::lock_guard lock{mtx};
    if (!mpfp && apicsInfo != nullptr) {
        mpfp = std::unique_ptr<cpu_mpfp>(new cpu_mpfp(reserved_mem));
        if (!mpfp->is_valid()) {
            mpfp = {};
        }
    }
    return mpfp ? &(*mpfp) : nullptr;
}

uint16_t ApStartup::GetIsaIoapicPin(uint8_t isaIrq) {
    if (madtptr) {
        return madtptr->GetIsaIoapicPin(isaIrq);
    } else {
        uint8_t isa_bus_id{0xFF};
        for (int i = 0; i < mpfp->get_num_bus(); i++) {
            const mp_bus_entry &bus = mpfp->get_bus(i);
            if (bus.bus_type[0] == 'I' && bus.bus_type[1] == 'S' && bus.bus_type[2] == 'A') {
                isa_bus_id = bus.bus_id;
            }
        }
        for (int i = 0; i < mpfp->get_num_ioapic_ints(); i++) {
            const mp_ioapic_interrupt_entry &ioapic_int = mpfp->get_ioapic_int(i);
            if (ioapic_int.source_bus_irq == isaIrq && ioapic_int.source_bus_id == isa_bus_id) {
                return ioapic_int.ioapic_intin;
            }
        }
        return isaIrq;
    }
}
