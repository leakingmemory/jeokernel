//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include "textconsole/b8000logger.h"
#include "TaskStateSegment.h"
#include "InterruptTaskState.h"
#include "InterruptDescriptorTable.h"
#include "interrupt.h"
#include "KernelElf.h"
#include <core/cpu_mpfp.h>
#include <core/LocalApic.h>
#include "Pic.h"
#include "IOApic.h"
#include <pagealloc.h>
#include <multiboot_impl.h>
#include <core/malloc.h>
#include <string.h>
#include <stack.h>
#include <vector>
#include <tuple>
#include <sstream>
#include <cpuid.h>
#include <string>
#include <concurrency/critical_section.h>
#include <mutex>
#include "PITTimerCalib.h"
#include "HardwareInterrupts.h"
#include "CpuInterrupts.h"
#include <core/vmem.h>
#include "start_ap.h"
#include "AcpiBoot.h"
#include "pci/pci.h"
#include "display/vga.h"
#include "pci/pci_bridge.h"
#include <core/scheduler.h>
#include <thread>
#include <core/nanotime.h>
#include <chrono>
#include <concurrency/raw_semaphore.h>
#include <devices/devices.h>
#include <devices/drivers.h>
#include "usb/usb_hcis.h"
#include "ps2/ps2.h"
#include "usb/usbifacedev.h"
#include "usb/usbkbd.h"
#include "ApStartup.h"

//#define THREADING_TESTS // Master switch
//#define FULL_SPEED_TESTS
//#define MICROSLEEP_TESTS
//#define SLEEP_TESTS
//#define SEMAPHORE_TEST // depends on sleep tests
//#define DISABLE_MPFP

void init_keyboard();

static const MultibootInfoHeader *multiboot_info = nullptr;
static normal_stack *stage1_stack = nullptr;
static GlobalDescriptorTable *gdt = nullptr;
static TaskStateSegment *glob_tss[TSS_MAX_CPUS] = {};
static InterruptTaskState *glob_int_task_state[TSS_MAX_CPUS] = {};
static InterruptDescriptorTable *idt = nullptr;
static KernelElf *kernel_elf = nullptr;
static hw_spinlock *aptimer_lock = nullptr;
static uint64_t timer_ticks = 0;
static uint64_t lapic_100ms = 0;
static cpu_mpfp *mpfp = nullptr;
static tasklist *scheduler = nullptr;
static ApStartup *apStartup = nullptr;

extern "C" {
    uint64_t bootstrap_stackptr = 0;
}

const MultibootInfoHeader &get_multiboot2(vmem &vm) {
    uint64_t base_addr = (uint64_t) multiboot_info;
    uint64_t offset = base_addr & 0x0FFF;
    base_addr -= offset;
    vm.page(0).rmap(base_addr);
    vm.page(1).rmap(base_addr + 0x1000);
    vm.page(2).rmap(base_addr + 0x2000);
    vm.page(3).rmap(base_addr + 0x3000);
    return *((const MultibootInfoHeader *) (((uint64_t )vm.pointer()) + offset));
}

const KernelElf &get_kernel_elf() {
    return *kernel_elf;
}

void set_tss(int cpun, struct TaskStateSegment *tss) {
    glob_tss[cpun] = tss;
}
TaskStateSegment *get_tss(int cpun) {
    return glob_tss[cpun + 1];
}

void set_its(int cpun, struct InterruptTaskState *its) {
    glob_int_task_state[cpun] = its;
}
InterruptTaskState *get_its(int cpun) {
    return glob_int_task_state[cpun + 1];
}

cpu_mpfp *get_mpfp() {
    return mpfp;
}

GlobalDescriptorTable *get_gdt() {
    return gdt;
}

InterruptDescriptorTable *get_idt() {
    return idt;
}

uint64_t get_lapic_100ms() {
    return lapic_100ms;
}

tasklist *get_scheduler() {
    return scheduler;
}

uint64_t get_ticks() {
    uint64_t ticks{0};
    {
        std::lock_guard lock{*aptimer_lock};
        ticks = timer_ticks;
    }
    return ticks;
}

ApStartup *GetApStartup() {
    return apStartup;
}

#define _get_pml4t()  (*((pagetable *) 0x1000))

void vmem_graph() {
    auto &pt = _get_pml4t()[0].get_subtable()[0].get_subtable()[0].get_subtable();
    for (int i = 0; i < 512; i++) {
        char *addr = (char *) ((i << 1) + 0xb8000);
        if (pt[i].os_virt_avail) {
            if (pt[i].os_phys_avail) {
                *addr = '+';
            } else {
                *addr = 'V';
            }
        } else if (pt[i].os_phys_avail) {
            *addr = 'P';
        } else {
            *addr = '-';
        }
    }
}

pagetable *allocate_pageentr() {
    static_assert(sizeof(pagetable) == 4096);
    uint64_t addr = pv_fix_pagealloc(4096);
    if (addr != 0) {
        return (pagetable *) memset((void *) addr, 0, 4096);
    } else {
        return nullptr;
    }
}

extern "C" {

    void start_m64(const MultibootInfoHeader *multibootInfoHeaderPtr, uint64_t stackptr) {
        multiboot_info = multibootInfoHeaderPtr;
        bootstrap_stackptr = stackptr; // For reuse with AP startup
        /*
         * Let's try to alloc a stack
         */
        initialize_pagetable_control();
        setup_simplest_malloc_impl();

        stage1_stack = new normal_stack;
        uint64_t stack = stage1_stack->get_addr();
        asm("mov %0, %%rax; mov %%rax,%%rsp; jmp secondboot; hlt;" :: "r"(stack) : "%rax");
    }

    void init_m64() {
        b8000logger *b8000Logger = new b8000logger();

        set_klogger(b8000Logger);

        std::vector<std::tuple<uint64_t,uint64_t>> reserved_mem{};

        vmem multiboot_vm(16384);
        const auto &multiboot2 = get_multiboot2(multiboot_vm);

        {
            get_klogger() << "Multiboot2 info at " << ((uint64_t) &multiboot2) << "\n";
            if (!multiboot2.has_parts()) {
                get_klogger() << "Error: Multiboot2 structure has no information\n";
                while (1) {
                    asm("hlt");
                }
            }
            uint64_t phys_mem_watermark = 0x2800000;
            uint64_t end_phys_addr = 0x2800000;
            {
                uint64_t phys_mem_added = 0;
                const auto *part = multiboot2.first_part();
                do {
                    if (part->type == 6) {
                        get_klogger() << "Memory map:\n";
                        auto &pml4t = _get_pml4t();
                        const auto &memoryMap = part->get_type6();
                        int num = memoryMap.get_num_entries();
                        uint64_t phys_mem_watermark_next = phys_mem_watermark;
                        bool first_pass = true;
                        uint64_t memory_extended = 0;
                        do {
                            if (!first_pass) {
                                get_klogger() << "Adding more physical memory:\n";
                            }
                            phys_mem_added = 0;
                            for (int i = 0; i < num; i++) {
                                const auto &entr = memoryMap.get_entry(i);
                                if (first_pass) {
                                    get_klogger() << " - Region " << entr.base_addr << " (" << entr.length << ") type "
                                                  << entr.type << "\n";
                                }
                                if (entr.type == 1) {
                                    uint64_t region_end = entr.base_addr + entr.length;
                                    if (region_end > end_phys_addr) {
                                        end_phys_addr = region_end;
                                    }
                                    if (region_end > phys_mem_watermark) {
                                        uint64_t start = entr.base_addr >= phys_mem_watermark ? entr.base_addr
                                                                                              : phys_mem_watermark;
                                        uint64_t last = start + 1;
                                        get_klogger() << "   - Added " << start << " - ";
                                        for (uint64_t addr = start;
                                             addr < (entr.base_addr + entr.length); addr += 0x1000) {
                                            pageentr *pe = get_pageentr64(pml4t, addr);
                                            if (pe == nullptr) {
                                                break;
                                            }
                                            pe->os_phys_avail = 1;
                                            last = addr + 0x1000;
                                            if (last > phys_mem_watermark_next) {
                                                phys_mem_watermark_next = last;
                                            }
                                            phys_mem_added += 0x1000;
                                        }
                                        --last;
                                        get_klogger() << last << "\n";
                                    }
                                } else if (first_pass) {
                                    uint64_t start = entr.base_addr >= phys_mem_watermark ? entr.base_addr
                                                                                          : phys_mem_watermark;
                                    for (uint64_t addr = start;
                                         addr < (entr.base_addr + entr.length); addr += 0x1000) {
                                        pageentr *pe = get_pageentr64(pml4t, addr);
                                        if (pe == nullptr) {
                                            break;
                                        }
                                        pe->os_phys_avail = 0;
                                        phys_mem_added += 0x1000;
                                    }
                                    reserved_mem.push_back(std::make_tuple<uint64_t,uint64_t>(entr.base_addr,entr.length));
                                }
                            }
                            if (phys_mem_watermark != phys_mem_watermark_next || phys_mem_added != 0) {
                                phys_mem_watermark = phys_mem_watermark_next;
                                get_klogger() << "Added " << phys_mem_added << " bytes, watermark "
                                              << phys_mem_watermark
                                              << ", end " << end_phys_addr << "\n";
                            }
                            uint32_t mem_ext_consumed = 0;
                            memory_extended = 0;
                            for (int i = 0; i < 512; i++) {
                                if (pml4t[i].present == 0) {
                                    pagetable *pt = allocate_pageentr();
                                    if (pt == nullptr) {
                                        break;
                                    }
                                    pml4t[i].page_ppn = (((uint64_t) pt) >> 12);
                                    pml4t[i].writeable = 1;
                                    pml4t[i].present = 1;
                                    mem_ext_consumed += 4096;
                                }
                                auto &pdpt = pml4t[i].get_subtable();
                                for (int j = 0; j < 512; j++) {
                                    if (pdpt[j].present == 0) {
                                        pagetable *pt = allocate_pageentr();
                                        if (pt == nullptr) {
                                            goto done_with_mem_extension;
                                        }
                                        pdpt[j].page_ppn = (((uint64_t) pt) >> 12);
                                        pdpt[j].writeable = 1;
                                        pdpt[j].present = 1;
                                        mem_ext_consumed += 4096;
                                    }
                                    auto &pdt = pdpt[j].get_subtable();
                                    for (int k = 0; k < 512; k++) {
                                        if (pdt[k].present == 0) {
                                            pagetable *pt = allocate_pageentr();
                                            if (pt == nullptr) {
                                                goto done_with_mem_extension;
                                            }
                                            for (int l = 0; l < 512; l++) {
                                                (*pt)[l].os_virt_avail = 1;
                                            }
                                            pdt[k].page_ppn = (((uint64_t) pt) >> 12);
                                            pdt[k].writeable = 1;
                                            pdt[k].present = 1;
                                            mem_ext_consumed += 4096;
                                            memory_extended += 512 * 4096;
                                        }
                                        uint64_t addr = i;
                                        addr = (addr << 9) + j;
                                        addr = (addr << 9) + k;
                                        addr = addr << (9 + 12);
                                        if (addr > end_phys_addr) {
                                            goto done_with_mem_extension;
                                        }
                                    }
                                }
                            }
done_with_mem_extension:
                            if (memory_extended != 0 || mem_ext_consumed != 0) {
                                get_klogger() << "Virtual memory extended by " << memory_extended << ", consumed "
                                              << mem_ext_consumed << "\n";
                            }
                            first_pass = false;
                        } while (phys_mem_added > 0 || memory_extended > 0);
                        get_klogger() << "Done with mapping physical memory\n";
                    }
                    if (part->hasNext(multiboot2)) {
                        part = part->next();
                    } else {
                        part = nullptr;
                    }
                } while (part != nullptr);
            }
        }

        for (const std::tuple<uint64_t,uint64_t> &res_mem : reserved_mem) {
            uint64_t base_addr = std::get<0>(res_mem);
            uint64_t end_addr = base_addr + std::get<1>(res_mem) - 1;
            get_klogger() << "Reserved memory range " << base_addr << " - " << end_addr << "\n";
        }

        create_hw_interrupt_handler();
        create_cpu_interrupt_handler();

        gdt = new GlobalDescriptorTable;

        TaskStateSegment *tss = new TaskStateSegment;
        InterruptTaskState *int_task_state = new InterruptTaskState(*tss);

        set_tss(0, tss);
        set_its(0, int_task_state);
        tss->install_bsp(*gdt);

        uint64_t *gdt_ptr = (uint64_t *) &(gdt->get_descriptor(0));
        get_klogger() << "GDT at " << ((uint64_t) gdt_ptr) << "\n";
        gdt->reload();

        uint64_t *tss_ptr = (uint64_t *) &(tss->get_entry());

        tss->install_bsp_cpu(*gdt);

        idt = new InterruptDescriptorTable;
        idt->interrupt_handler(0x00, 0x8, (uint64_t) interrupt_imm00_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x01, 0x8, (uint64_t) interrupt_imm01_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x02, 0x8, (uint64_t) interrupt_imm02_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x03, 0x8, (uint64_t) interrupt_imm03_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x04, 0x8, (uint64_t) interrupt_imm04_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x05, 0x8, (uint64_t) interrupt_imm05_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x06, 0x8, (uint64_t) interrupt_imm06_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x07, 0x8, (uint64_t) interrupt_imm07_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x08, 0x8, (uint64_t) interrupt_imm08_handler, 1, 0xF, 0);
        idt->interrupt_handler(0x09, 0x8, (uint64_t) interrupt_imm09_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x0A, 0x8, (uint64_t) interrupt_imm0A_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x0B, 0x8, (uint64_t) interrupt_imm0B_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x0C, 0x8, (uint64_t) interrupt_imm0C_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x0D, 0x8, (uint64_t) interrupt_imm0D_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x0E, 0x8, (uint64_t) interrupt_imm0E_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x0F, 0x8, (uint64_t) interrupt_imm0F_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x10, 0x8, (uint64_t) interrupt_imm10_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x11, 0x8, (uint64_t) interrupt_imm11_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x12, 0x8, (uint64_t) interrupt_imm12_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x13, 0x8, (uint64_t) interrupt_imm13_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x14, 0x8, (uint64_t) interrupt_imm14_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x15, 0x8, (uint64_t) interrupt_imm15_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x16, 0x8, (uint64_t) interrupt_imm16_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x17, 0x8, (uint64_t) interrupt_imm17_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x18, 0x8, (uint64_t) interrupt_imm18_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x19, 0x8, (uint64_t) interrupt_imm19_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x1A, 0x8, (uint64_t) interrupt_imm1A_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x1B, 0x8, (uint64_t) interrupt_imm1B_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x1C, 0x8, (uint64_t) interrupt_imm1C_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x1D, 0x8, (uint64_t) interrupt_imm1D_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x1E, 0x8, (uint64_t) interrupt_imm1E_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x1F, 0x8, (uint64_t) interrupt_imm1F_handler, 0, 0xF, 0);
        idt->interrupt_handler(0x20, 0x8, (uint64_t) interrupt_imm20_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x21, 0x8, (uint64_t) interrupt_imm21_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x22, 0x8, (uint64_t) interrupt_imm22_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x23, 0x8, (uint64_t) interrupt_imm23_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x24, 0x8, (uint64_t) interrupt_imm24_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x25, 0x8, (uint64_t) interrupt_imm25_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x26, 0x8, (uint64_t) interrupt_imm26_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x27, 0x8, (uint64_t) interrupt_imm27_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x28, 0x8, (uint64_t) interrupt_imm28_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x29, 0x8, (uint64_t) interrupt_imm29_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x2A, 0x8, (uint64_t) interrupt_imm2A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x2B, 0x8, (uint64_t) interrupt_imm2B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x2C, 0x8, (uint64_t) interrupt_imm2C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x2D, 0x8, (uint64_t) interrupt_imm2D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x2E, 0x8, (uint64_t) interrupt_imm2E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x2F, 0x8, (uint64_t) interrupt_imm2F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x30, 0x8, (uint64_t) interrupt_imm30_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x31, 0x8, (uint64_t) interrupt_imm31_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x32, 0x8, (uint64_t) interrupt_imm32_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x33, 0x8, (uint64_t) interrupt_imm33_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x34, 0x8, (uint64_t) interrupt_imm34_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x35, 0x8, (uint64_t) interrupt_imm35_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x36, 0x8, (uint64_t) interrupt_imm36_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x37, 0x8, (uint64_t) interrupt_imm37_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x38, 0x8, (uint64_t) interrupt_imm38_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x39, 0x8, (uint64_t) interrupt_imm39_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x3A, 0x8, (uint64_t) interrupt_imm3A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x3B, 0x8, (uint64_t) interrupt_imm3B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x3C, 0x8, (uint64_t) interrupt_imm3C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x3D, 0x8, (uint64_t) interrupt_imm3D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x3E, 0x8, (uint64_t) interrupt_imm3E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x3F, 0x8, (uint64_t) interrupt_imm3F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x40, 0x8, (uint64_t) interrupt_imm40_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x41, 0x8, (uint64_t) interrupt_imm41_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x42, 0x8, (uint64_t) interrupt_imm42_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x43, 0x8, (uint64_t) interrupt_imm43_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x44, 0x8, (uint64_t) interrupt_imm44_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x45, 0x8, (uint64_t) interrupt_imm45_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x46, 0x8, (uint64_t) interrupt_imm46_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x47, 0x8, (uint64_t) interrupt_imm47_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x48, 0x8, (uint64_t) interrupt_imm48_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x49, 0x8, (uint64_t) interrupt_imm49_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x4A, 0x8, (uint64_t) interrupt_imm4A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x4B, 0x8, (uint64_t) interrupt_imm4B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x4C, 0x8, (uint64_t) interrupt_imm4C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x4D, 0x8, (uint64_t) interrupt_imm4D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x4E, 0x8, (uint64_t) interrupt_imm4E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x4F, 0x8, (uint64_t) interrupt_imm4F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x50, 0x8, (uint64_t) interrupt_imm50_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x51, 0x8, (uint64_t) interrupt_imm51_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x52, 0x8, (uint64_t) interrupt_imm52_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x53, 0x8, (uint64_t) interrupt_imm53_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x54, 0x8, (uint64_t) interrupt_imm54_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x55, 0x8, (uint64_t) interrupt_imm55_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x56, 0x8, (uint64_t) interrupt_imm56_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x57, 0x8, (uint64_t) interrupt_imm57_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x58, 0x8, (uint64_t) interrupt_imm58_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x59, 0x8, (uint64_t) interrupt_imm59_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x5A, 0x8, (uint64_t) interrupt_imm5A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x5B, 0x8, (uint64_t) interrupt_imm5B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x5C, 0x8, (uint64_t) interrupt_imm5C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x5D, 0x8, (uint64_t) interrupt_imm5D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x5E, 0x8, (uint64_t) interrupt_imm5E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x5F, 0x8, (uint64_t) interrupt_imm5F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x60, 0x8, (uint64_t) interrupt_imm60_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x61, 0x8, (uint64_t) interrupt_imm61_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x62, 0x8, (uint64_t) interrupt_imm62_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x63, 0x8, (uint64_t) interrupt_imm63_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x64, 0x8, (uint64_t) interrupt_imm64_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x65, 0x8, (uint64_t) interrupt_imm65_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x66, 0x8, (uint64_t) interrupt_imm66_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x67, 0x8, (uint64_t) interrupt_imm67_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x68, 0x8, (uint64_t) interrupt_imm68_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x69, 0x8, (uint64_t) interrupt_imm69_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x6A, 0x8, (uint64_t) interrupt_imm6A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x6B, 0x8, (uint64_t) interrupt_imm6B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x6C, 0x8, (uint64_t) interrupt_imm6C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x6D, 0x8, (uint64_t) interrupt_imm6D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x6E, 0x8, (uint64_t) interrupt_imm6E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x6F, 0x8, (uint64_t) interrupt_imm6F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x70, 0x8, (uint64_t) interrupt_imm70_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x71, 0x8, (uint64_t) interrupt_imm71_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x72, 0x8, (uint64_t) interrupt_imm72_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x73, 0x8, (uint64_t) interrupt_imm73_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x74, 0x8, (uint64_t) interrupt_imm74_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x75, 0x8, (uint64_t) interrupt_imm75_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x76, 0x8, (uint64_t) interrupt_imm76_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x77, 0x8, (uint64_t) interrupt_imm77_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x78, 0x8, (uint64_t) interrupt_imm78_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x79, 0x8, (uint64_t) interrupt_imm79_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x7A, 0x8, (uint64_t) interrupt_imm7A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x7B, 0x8, (uint64_t) interrupt_imm7B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x7C, 0x8, (uint64_t) interrupt_imm7C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x7D, 0x8, (uint64_t) interrupt_imm7D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x7E, 0x8, (uint64_t) interrupt_imm7E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x7F, 0x8, (uint64_t) interrupt_imm7F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x80, 0x8, (uint64_t) interrupt_imm80_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x81, 0x8, (uint64_t) interrupt_imm81_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x82, 0x8, (uint64_t) interrupt_imm82_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x83, 0x8, (uint64_t) interrupt_imm83_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x84, 0x8, (uint64_t) interrupt_imm84_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x85, 0x8, (uint64_t) interrupt_imm85_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x86, 0x8, (uint64_t) interrupt_imm86_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x87, 0x8, (uint64_t) interrupt_imm87_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x88, 0x8, (uint64_t) interrupt_imm88_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x89, 0x8, (uint64_t) interrupt_imm89_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x8A, 0x8, (uint64_t) interrupt_imm8A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x8B, 0x8, (uint64_t) interrupt_imm8B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x8C, 0x8, (uint64_t) interrupt_imm8C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x8D, 0x8, (uint64_t) interrupt_imm8D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x8E, 0x8, (uint64_t) interrupt_imm8E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x8F, 0x8, (uint64_t) interrupt_imm8F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x90, 0x8, (uint64_t) interrupt_imm90_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x91, 0x8, (uint64_t) interrupt_imm91_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x92, 0x8, (uint64_t) interrupt_imm92_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x93, 0x8, (uint64_t) interrupt_imm93_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x94, 0x8, (uint64_t) interrupt_imm94_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x95, 0x8, (uint64_t) interrupt_imm95_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x96, 0x8, (uint64_t) interrupt_imm96_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x97, 0x8, (uint64_t) interrupt_imm97_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x98, 0x8, (uint64_t) interrupt_imm98_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x99, 0x8, (uint64_t) interrupt_imm99_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x9A, 0x8, (uint64_t) interrupt_imm9A_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x9B, 0x8, (uint64_t) interrupt_imm9B_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x9C, 0x8, (uint64_t) interrupt_imm9C_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x9D, 0x8, (uint64_t) interrupt_imm9D_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x9E, 0x8, (uint64_t) interrupt_imm9E_handler, 0, 0xE, 0);
        idt->interrupt_handler(0x9F, 0x8, (uint64_t) interrupt_imm9F_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA0, 0x8, (uint64_t) interrupt_immA0_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA1, 0x8, (uint64_t) interrupt_immA1_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA2, 0x8, (uint64_t) interrupt_immA2_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA3, 0x8, (uint64_t) interrupt_immA3_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA4, 0x8, (uint64_t) interrupt_immA4_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA5, 0x8, (uint64_t) interrupt_immA5_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA6, 0x8, (uint64_t) interrupt_immA6_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA7, 0x8, (uint64_t) interrupt_immA7_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA8, 0x8, (uint64_t) interrupt_immA8_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xA9, 0x8, (uint64_t) interrupt_immA9_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xAA, 0x8, (uint64_t) interrupt_immAA_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xAB, 0x8, (uint64_t) interrupt_immAB_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xAC, 0x8, (uint64_t) interrupt_immAC_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xAD, 0x8, (uint64_t) interrupt_immAD_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xAE, 0x8, (uint64_t) interrupt_immAE_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xAF, 0x8, (uint64_t) interrupt_immAF_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB0, 0x8, (uint64_t) interrupt_immB0_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB1, 0x8, (uint64_t) interrupt_immB1_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB2, 0x8, (uint64_t) interrupt_immB2_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB3, 0x8, (uint64_t) interrupt_immB3_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB4, 0x8, (uint64_t) interrupt_immB4_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB5, 0x8, (uint64_t) interrupt_immB5_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB6, 0x8, (uint64_t) interrupt_immB6_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB7, 0x8, (uint64_t) interrupt_immB7_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB8, 0x8, (uint64_t) interrupt_immB8_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xB9, 0x8, (uint64_t) interrupt_immB9_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xBA, 0x8, (uint64_t) interrupt_immBA_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xBB, 0x8, (uint64_t) interrupt_immBB_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xBC, 0x8, (uint64_t) interrupt_immBC_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xBD, 0x8, (uint64_t) interrupt_immBD_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xBE, 0x8, (uint64_t) interrupt_immBE_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xBF, 0x8, (uint64_t) interrupt_immBF_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC0, 0x8, (uint64_t) interrupt_immC0_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC1, 0x8, (uint64_t) interrupt_immC1_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC2, 0x8, (uint64_t) interrupt_immC2_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC3, 0x8, (uint64_t) interrupt_immC3_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC4, 0x8, (uint64_t) interrupt_immC4_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC5, 0x8, (uint64_t) interrupt_immC5_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC6, 0x8, (uint64_t) interrupt_immC6_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC7, 0x8, (uint64_t) interrupt_immC7_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC8, 0x8, (uint64_t) interrupt_immC8_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xC9, 0x8, (uint64_t) interrupt_immC9_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xCA, 0x8, (uint64_t) interrupt_immCA_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xCB, 0x8, (uint64_t) interrupt_immCB_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xCC, 0x8, (uint64_t) interrupt_immCC_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xCD, 0x8, (uint64_t) interrupt_immCD_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xCE, 0x8, (uint64_t) interrupt_immCE_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xCF, 0x8, (uint64_t) interrupt_immCF_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD0, 0x8, (uint64_t) interrupt_immD0_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD1, 0x8, (uint64_t) interrupt_immD1_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD2, 0x8, (uint64_t) interrupt_immD2_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD3, 0x8, (uint64_t) interrupt_immD3_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD4, 0x8, (uint64_t) interrupt_immD4_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD5, 0x8, (uint64_t) interrupt_immD5_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD6, 0x8, (uint64_t) interrupt_immD6_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD7, 0x8, (uint64_t) interrupt_immD7_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD8, 0x8, (uint64_t) interrupt_immD8_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xD9, 0x8, (uint64_t) interrupt_immD9_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xDA, 0x8, (uint64_t) interrupt_immDA_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xDB, 0x8, (uint64_t) interrupt_immDB_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xDC, 0x8, (uint64_t) interrupt_immDC_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xDD, 0x8, (uint64_t) interrupt_immDD_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xDE, 0x8, (uint64_t) interrupt_immDE_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xDF, 0x8, (uint64_t) interrupt_immDF_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE0, 0x8, (uint64_t) interrupt_immE0_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE1, 0x8, (uint64_t) interrupt_immE1_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE2, 0x8, (uint64_t) interrupt_immE2_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE3, 0x8, (uint64_t) interrupt_immE3_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE4, 0x8, (uint64_t) interrupt_immE4_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE5, 0x8, (uint64_t) interrupt_immE5_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE6, 0x8, (uint64_t) interrupt_immE6_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE7, 0x8, (uint64_t) interrupt_immE7_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE8, 0x8, (uint64_t) interrupt_immE8_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xE9, 0x8, (uint64_t) interrupt_immE9_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xEA, 0x8, (uint64_t) interrupt_immEA_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xEB, 0x8, (uint64_t) interrupt_immEB_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xEC, 0x8, (uint64_t) interrupt_immEC_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xED, 0x8, (uint64_t) interrupt_immED_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xEE, 0x8, (uint64_t) interrupt_immEE_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xEF, 0x8, (uint64_t) interrupt_immEF_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF0, 0x8, (uint64_t) interrupt_immF0_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF1, 0x8, (uint64_t) interrupt_immF1_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF2, 0x8, (uint64_t) interrupt_immF2_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF3, 0x8, (uint64_t) interrupt_immF3_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF4, 0x8, (uint64_t) interrupt_immF4_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF5, 0x8, (uint64_t) interrupt_immF5_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF6, 0x8, (uint64_t) interrupt_immF6_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF7, 0x8, (uint64_t) interrupt_immF7_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF8, 0x8, (uint64_t) interrupt_immF8_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xF9, 0x8, (uint64_t) interrupt_immF9_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xFA, 0x8, (uint64_t) interrupt_immFA_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xFB, 0x8, (uint64_t) interrupt_immFB_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xFC, 0x8, (uint64_t) interrupt_immFC_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xFD, 0x8, (uint64_t) interrupt_immFD_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xFE, 0x8, (uint64_t) interrupt_immFE_handler, 0, 0xE, 0);
        idt->interrupt_handler(0xFF, 0x8, (uint64_t) interrupt_immFF_handler, 0, 0xE, 0);

        idt->install();

        uint64_t *idt_ptr = (uint64_t *) &(idt->idt());
        get_klogger() << "Interrupt table at " << ((uint64_t) idt_ptr) << "\n";
        /*for (int i = 0; i < 16; i++) {
            get_klogger() << idt_ptr[i << 1] << " " << idt_ptr[(i << 1) + 1] << "\n";
        }*/

        kernel_elf = new KernelElf(multiboot2);

#if 0
        get_klogger() << "ELF Sections:\n";
        {
            const ELF64_header &elfhdr = kernel_elf->elf().get_elf64_header();
            const char *strtab = elfhdr.get_strtab();
            for (uint16_t i = 0; i < elfhdr.e_shnum; i++) {
                const ELF64_section_entry &se = elfhdr.get_section_entry(i);
                char flags[5];
                flags[4] = 0;
                flags[3] = se.sh_flags & SHF_STRINGS ? 'S' : ' ';
                flags[2] = se.sh_flags & SHF_EXECINSTR ? 'E' : ' ';
                flags[1] = se.sh_flags & SHF_WRITE ? 'W' : ' ';
                flags[0] = se.sh_flags & SHF_ALLOC ? 'A' : ' ';
                get_klogger() << se.sh_type << " " << &(flags[0]) << " " << (uint32_t) se.sh_offset << " " << (uint32_t) se.sh_addr << " " << (uint32_t) se.sh_size << " " << (strtab + se.sh_name) << "\n";
            }
            const auto *symtab = elfhdr.get_symtab();
            if (symtab != nullptr) {
                get_klogger() << "Symbols: " << symtab->sh_offset << " " << symtab->sh_size << "\n";
                for (uint32_t i = 0; i < elfhdr.get_symbols(*symtab); i++) {
                    const ELF64_symbol_entry &sym = elfhdr.get_symbol(*symtab, i);
                    get_klogger() << sym.st_value << " " << sym.st_size << " " << (strtab + sym.st_name) << "\n";
                }
            }
        }
#endif

        scheduler = new tasklist;
        {
            scheduler->create_current_idle_task(0);
        }

#ifndef DISABLE_MPFP
        mpfp = new cpu_mpfp{reserved_mem};
        if (!mpfp->is_valid()) {
            delete mpfp;
            mpfp = nullptr;
        }
#else
        mpfp = nullptr;
#endif
        LocalApic lapic{mpfp};
        {

            /*int cpu_idx = lapic.get_cpu_num(*mpfp);

            get_klogger() << "Current cpu lapic reg " << lapic.get_lapic_id() << " num=" << (uint8_t) cpu_idx << "\n";

            if (cpu_idx < 0) {
                wild_panic("cpu configuration was not found or recongized");
            }

            auto &cpu = mpfp->get_cpu(cpu_idx);

            if ((cpu.features & 0x200) == 0) {
                wild_panic("system lacks apic support");
            }*/

            Pic pic{};
            pic.disable();
            pic.remap(0x20);
            pic.disable();

            lapic.enable_apic();
            lapic.set_lint0_int(0x21, false);
            lapic.set_lint1_int(0x22, false);

            for (int i = 0; i < 3; i++) {
                get_hw_interrupt_handler().add_finalizer(i, [&lapic](Interrupt &interrupt) {
                    lapic.eio();
                });
            }
        }

        PITTimerCalib calib_timer{};
        {
            lapic.set_timer_div(0x3);
            {
                lapic_100ms = (uint64_t) 0x00FFFFFFFF;
                lapic.set_timer_count((uint32_t) lapic_100ms);
                set_nanotime_ref(0);
                for (int i = 0; i < 10; i++) {
                    calib_timer.delay(10000);
                }
                lapic.set_timer_int_mode(0x20, 0x10000); // Disable
                lapic_100ms -= lapic.get_timer_value();
                lapic.set_timer_count((uint32_t) (lapic_100ms / 10));
                set_nanotime_ref(100000000);
            }
            {
                std::stringstream sstr{};
                sstr << std::dec << "TSC frequency " << (get_tsc_frequency() / 1000) << "KHz, uptime nanos " << get_nanotime_ref() << "\n";
                get_klogger() << sstr.str().c_str();
            }

            {
                std::stringstream stream{};
                stream << std::dec << " " << lapic_100ms << " ticks per 100ms";
                std::string info = stream.str();
                get_klogger() << "timer: " << lapic.set_timer_int_mode(0x20, LAPIC_TIMER_PERIODIC) << info.c_str()
                              << "\n";
                lapic.set_timer_count((uint32_t) (lapic_100ms / 10));
            }

            timer_ticks = 0;
            const char *characters = "|/-\\|/-\\";
            aptimer_lock = new hw_spinlock;
            uint64_t *aptimers = (uint64_t *) malloc(sizeof(aptimers[0]) * MAX_CPUs);
            for (int i = 0; i < MAX_CPUs; i++) {
                aptimers[i] = 0;
            }
            get_hw_interrupt_handler().add_handler(0, [&lapic, characters, aptimers](Interrupt &interrupt) {
                bool multicpu = scheduler->is_multicpu();
                uint8_t cpu_num = multicpu ? lapic.get_cpu_num(*mpfp) : 0;
                bool is_bootstrap_cpu = !multicpu || (mpfp->get_cpu(cpu_num).cpu_flags & 2) != 0;
                uint64_t prev_vis = 0;
                uint64_t vis = 0;
                uint64_t ticks = 0;

                reload_pagetables();

                {
                    std::lock_guard lock{*aptimer_lock};

                    prev_vis = (is_bootstrap_cpu ? timer_ticks : aptimers[cpu_num]) >> 5;
                    if (is_bootstrap_cpu) {
                        ticks = ++timer_ticks;
                    } else {
                        ticks = ++aptimers[cpu_num];
                    }
                    vis = ticks >> 5;
                }
                if (is_bootstrap_cpu) {
                    scheduler->event(TASK_EVENT_TIMER100HZ, ticks, 0);
                }
                if (prev_vis != vis) {
                    char str[2] = {characters[vis & 7], 0};
                    uint8_t pos = 79 - cpu_num;
                    get_klogger().print_at(pos, 24, &(str[0]));
                }
            });

            get_hw_interrupt_handler().add_handler(0, [&lapic] (Interrupt &intr) {
                bool multicpu = scheduler->is_multicpu();
                scheduler->switch_tasks(intr, multicpu ? lapic.get_cpu_num(*mpfp) : 0);
            });

        }

        asm("sti");

        using namespace std::literals::chrono_literals;
        std::mutex cons_mtx{};

        std::thread clock_thread{[&cons_mtx] () {
            while (true) {
                std::stringstream clocktxt{};
                clocktxt << std::dec << " T+";
                uint64_t tm = get_nanotime_ref();
                uint64_t sec = tm / 1000000000;
                uint64_t mins = sec / 60;
                sec = sec % 60;
                uint64_t hours = mins / 60;
                mins = mins % 60;
                if (hours < 10) {
                    clocktxt << "0";
                }
                clocktxt << hours << ":";
                if (mins < 10) {
                    clocktxt << "0";
                }
                clocktxt << mins << ":";
                if (sec < 10) {
                    clocktxt << "0";
                }
                clocktxt << sec << " ";
                {
                    std::lock_guard lock{cons_mtx};
                    get_klogger().print_at(60, 0, clocktxt.str().c_str());
                }
                std::this_thread::sleep_for(20ms);
            }
        }};
        clock_thread.detach();

        init_pci();
        init_devices();
        init_keyboard();
        get_drivers().AddDriver(new vga_driver());
        get_drivers().AddDriver(new pci_bridge_driver());
        get_drivers().AddDriver(new uhci_driver());
        get_drivers().AddDriver(new ohci_driver());
        get_drivers().AddDriver(new ehci_driver());
        get_drivers().AddDriver(new xhci_driver());
        get_drivers().AddDriver(new usbifacedev_driver());
        get_drivers().AddDriver(new usbkbd_driver());

        AcpiBoot acpi_boot{multiboot2};

        {
            std::thread pci_scan_thread{[&acpi_boot, &calib_timer, tss, int_task_state]() {
                acpi_boot.join();

                apStartup = new ApStartup(gdt, mpfp, tss, int_task_state);
                apStartup->Init(&calib_timer);

                detect_root_pcis();
                if (acpi_boot.has_8042()) {
                    ps2 *ps2dev = new ps2();
                    devices().add(*ps2dev);
                    ps2dev->init();
                }
            }};
            pci_scan_thread.detach();
        }

        int cpu_num = lapic.get_cpu_num(*mpfp);

#ifdef THREADING_TESTS
#ifdef SLEEP_TESTS
#ifdef SEMAPHORE_TEST
        raw_semaphore semaphore1{-1000};
        raw_semaphore semaphore2{2000};
#endif
#endif
        std::thread joining_thread{[&cons_mtx
#ifdef SLEEP_TESTS
#ifdef SEMAPHORE_TEST
                                    , &semaphore1, &semaphore2
#endif
#endif
                                    ] () {
#ifdef MICROSLEEP_TESTS
            {
                std::thread nanosleep_thread{[&cons_mtx]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(0, 1, str.c_str());
                        }
                        ++counter;
                        std::this_thread::sleep_for(1ns);
                    }
                }};
                nanosleep_thread.detach();
            }
            {
                std::thread usleep_thread{[&cons_mtx]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(20, 1, str.c_str());
                        }
                        ++counter;
                        std::this_thread::sleep_for(1us);
                    }
                }};
                usleep_thread.detach();
            }
#endif
#ifdef SLEEP_TESTS
            {
                std::thread msleep_thread{[&cons_mtx
#ifdef SEMAPHORE_TEST
                                           , &semaphore1, &semaphore2
#endif
                                           ]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(40, 1, str.c_str());
                        }
                        ++counter;
                        std::this_thread::sleep_for(1ms);

#ifdef SEMAPHORE_TEST
                        semaphore1.release();
                        semaphore2.release();
#endif
                    }
                }};
                msleep_thread.detach();
            }
            {
                std::thread sleep_thread{[&cons_mtx]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(60, 1, str.c_str());
                        }
                        ++counter;
                        std::this_thread::sleep_for(1s);
                    }
                }};
                sleep_thread.detach();
            }
#ifdef SEMAPHORE_TEST
            {
                std::thread sem1_thread{[&cons_mtx, &semaphore1]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        semaphore1.acquire();
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(0, 2, str.c_str());
                        }
                        ++counter;
                    }
                }};
                std::thread sem2_thread{[&cons_mtx, &semaphore2]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        semaphore2.acquire();
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(20, 2, str.c_str());
                        }
                        ++counter;
                    }
                }};
                sem1_thread.detach();
                sem2_thread.detach();
            }
#endif
#endif

#ifdef FULL_SPEED_TESTS
            uint64_t counter = 0;
            {
                {
                    std::thread detach_thread{[&cons_mtx] () {
                        uint64_t counter = 0;
                        while (counter < 1000000) {
                            std::stringstream countstr{};
                            countstr << std::dec << counter;
                            std::string str = countstr.str();
                            {
                                std::lock_guard lock{cons_mtx};
                                get_klogger().print_at(40, 0, str.c_str());
                            }
                            ++counter;
                        }
                    }};
                    detach_thread.detach();
                }
                std::thread test_thread{[&cons_mtx]() {
                    uint64_t counter = 0;
                    while (counter < 1000000) {
                        std::stringstream countstr{};
                        countstr << std::dec << counter;
                        std::string str = countstr.str();
                        {
                            std::lock_guard lock{cons_mtx};
                            get_klogger().print_at(0, 0, str.c_str());
                        }
                        ++counter;
                    }
                }};
                while (counter < 200000) {
                    std::stringstream countstr{};
                    countstr << std::dec << counter;
                    std::string str = countstr.str();
                    {
                        std::lock_guard lock{cons_mtx};
                        get_klogger().print_at(20, 0, str.c_str());
                    }
                    ++counter;
                }
                test_thread.join();
                while (counter < 500000) {
                    std::stringstream countstr{};
                    countstr << std::dec << counter;
                    std::string str = countstr.str();
                    {
                        std::lock_guard lock{cons_mtx};
                        get_klogger().print_at(20, 0, str.c_str());
                    }
                    ++counter;
                }
            }

            while (counter < 1000000) {
                std::stringstream countstr{};
                countstr << std::dec << counter;
                std::string str = countstr.str();
                {
                    std::lock_guard lock{cons_mtx};
                    get_klogger().print_at(20, 0, str.c_str());
                }
                ++counter;
            }
#endif
        }};
#endif

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