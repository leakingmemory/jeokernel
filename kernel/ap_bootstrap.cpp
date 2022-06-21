//
// Created by sigsegv on 06.05.2021.
//

#include <klogger.h>
#include <stack.h>
#include <sstream>
#include "start_ap.h"
#include "TaskStateSegment.h"
#include <core/LocalApic.h>
#include "InterruptDescriptorTable.h"
#include "ApStartup.h"
#include "SyscallSupport.h"
#include <core/scheduler.h>
#include <core/nanotime.h>
#include <pagealloc.h>

TaskStateSegment *get_tss(int cpun);
GlobalDescriptorTable *get_gdt();
InterruptDescriptorTable *get_idt();
uint64_t get_lapic_100ms();

extern "C" {
    void ap_bootstrap() {
        //get_klogger() << "In AP\n";
        auto *ap_stack = new normal_stack;
        uint64_t stack = ap_stack->get_addr();
        asm("mov %0, %%rax; mov %%rax,%%rsp; mov %1, %%rdi; jmp ap_with_stack; hlt;" :: "r"(stack), "r"(ap_stack) : "%rax");
    }

    void ap_boot(normal_stack *stack) {
        /*
         * Finished using the bootstrap stack, so release for next AP
         * start.
         */
        get_ap_start_lock()->unlock();

        //get_klogger() << "In AP with stack\n";

        GlobalDescriptorTable *gdt = get_gdt();

        ApStartup *apStartup = GetApStartup();

        int cpu_num = apStartup->GetCpuNum();

        //get_klogger() << "Loading TSS/ITS for CPU "<< (uint8_t) cpu_num << "\n";

        TaskStateSegment *tss = get_tss(cpu_num);

        tss->install_cpu(*gdt, cpu_num);

        InterruptDescriptorTable *idt = get_idt();

        idt->install();

        auto *lapic = apStartup->GetLocalApic();

        lapic->enable_apic();
        {
            //uint32_t ltimer_mode =
            lapic->set_timer_int_mode(0x20, LAPIC_TIMER_PERIODIC);
            //get_klogger() << "AP init timer: " << ltimer_mode << "\n";
        }
        lapic->set_lint0_int(0x21, false);
        lapic->set_lint1_int(0x22, false);

        uint64_t lapic_100ms = get_lapic_100ms();

        lapic->set_timer_div(0x3);
        lapic->set_timer_count((uint32_t) (lapic_100ms / 10));

        {
            //uint32_t ltimer_mode =
            lapic->set_timer_int_mode(0x20, LAPIC_TIMER_PERIODIC);
            //std::stringstream stream{};
            //stream << std::dec << " " << lapic_100ms << " ticks per 100ms";
            //std::string info = stream.str();
            //get_klogger() << "timer: " << ltimer_mode << info.c_str()
            //              << "\n";
        }

        {
            std::stringstream name{};
            name << "[idle" << cpu_num << "]";
            get_scheduler()->create_current_idle_task(cpu_num);
            get_scheduler()->set_name(name.str());
        }

        {
            std::stringstream sstr{};
            sstr << std::dec << "Additional cpu started at nanos " << get_nanotime_ref() << "\n";
            get_klogger() << sstr.str().c_str();
        }

        lapic->eio();

        vmem_set_per_cpu_pagetables();

        SyscallSupport::Instance().CpuSetup();

        asm("sti");

        while (1) {
            asm("hlt");

            /* The return from interrupt-ed hlt goes here, the return is
             * complete, so stack quarantines for this cpu can be cleared.
             * THis is the idle task, and pinned to this cpu, so cpu_num
             * should be valid.
             * */
            if (get_scheduler()->clear_stack_quarantines(cpu_num)) {
                asm("int $0xFE");
            }
        }
    }
}