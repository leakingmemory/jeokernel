//
// Created by sigsegv on 06.05.2021.
//

#include <klogger.h>
#include <stack.h>
#include <sstream>
#include "start_ap.h"
#include "TaskStateSegment.h"
#include "cpu_mpfp.h"
#include "LocalApic.h"
#include "InterruptDescriptorTable.h"
#include <core/scheduler.h>

TaskStateSegment *get_tss(int cpun);
cpu_mpfp *get_mpfp();
GlobalDescriptorTable *get_gdt();
InterruptDescriptorTable *get_idt();
uint64_t get_lapic_100ms();

extern "C" {
    void ap_bootstrap() {
        get_klogger() << "In AP\n";
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

        get_klogger() << "In AP with stack\n";

        cpu_mpfp *mpfp = get_mpfp();

        GlobalDescriptorTable *gdt = get_gdt();

        LocalApic lapic{*mpfp};

        int cpu_num = lapic.get_cpu_num(*mpfp);

        get_klogger() << "Loading TSS/ITS for CPU "<< (uint8_t) cpu_num << "\n";

        TaskStateSegment *tss = get_tss(cpu_num);

        tss->install_cpu(*gdt, cpu_num);

        InterruptDescriptorTable *idt = get_idt();

        idt->install();

        lapic.enable_apic();
        get_klogger() << "AP init timer: " << lapic.set_timer_int_mode(0x20, LAPIC_TIMER_PERIODIC) << "\n";
        lapic.set_lint0_int(0x21, false);
        lapic.set_lint1_int(0x22, false);

        uint64_t lapic_100ms = get_lapic_100ms();

        lapic.set_timer_div(0x3);
        lapic.set_timer_count((uint32_t) (lapic_100ms / 10));

        {
            std::stringstream stream{};
            stream << std::dec << " " << lapic_100ms << " ticks per 100ms";
            std::string info = stream.str();
            get_klogger() << "timer: " << lapic.set_timer_int_mode(0x20, LAPIC_TIMER_PERIODIC) << info.c_str()
                          << "\n";
        }

        get_scheduler()->create_current_idle_task(cpu_num);

        get_klogger() << "AP enable interrupts\n";

        lapic.eio();

        asm("sti");

        while (1) {
            asm("hlt");
        }
    }
}