//
// Created by sigsegv on 9/17/26.
//
#include "../../serial/uart.h"
#include "include/smp.h"
#include "null_klogger.h"
#include "bootstrap.h"
#include "dtb.h"
#include <physpagemap.h>
#include <pagealloc.h>
#include <core/malloc.h>
#include <vector>
#include <tuple>
#include "../../HardwareInterrupts.h"
#include "../../CpuInterrupts.h"
#include <interrupt.h>
#include <mutex>
#include "../../KernelElf.h"

#include "iostream"
#include "core/scheduler.h"

extern "C" void fpu_init();

static hw_spinlock *boot_lock{nullptr};
static int boot_stage_counter{0};
static int cpu_num_vector{0};
static KernelElf *kernel_elf = nullptr;
static tasklist *scheduler = nullptr;

tasklist *get_scheduler() {
    return scheduler;
}

static void early_init() {
    uart *spcom_cons{nullptr};
    int primary_console{0};
    if (get_cpu_num() == 0) {
        volatile void *uart = bootstrap_get_uart();
        if (uart != nullptr) {
            class uart u{uart};
            if (u.probe()) {
                spcom_cons = new class uart(std::move(u));
                primary_console = add_klogger(spcom_cons);
            } else {
                primary_console = add_klogger(new null_klogger());
            }
        } else {
            primary_console = add_klogger(new null_klogger());
        }
        (void) primary_console;
        get_klogger() << "cpu0: console output established\n";

        std::vector<std::tuple<uint64_t,uint64_t>> reserved_mem{};
        uint64_t dtb_addr = bootstrap_get_dtb();
        get_klogger() << "Device tree at " << dtb_addr << "\n";

        class dtb devtree{dtb_addr};
        if (!devtree.is_valid()) {
            get_klogger() << "Error: Memory map from loader (DTB) is missing or empty\n";
            while (1) {
                asm volatile("wfe");
            }
        }

        auto memory_banks = devtree.get_memory_banks();
        auto reserved_regions = devtree.get_reserved_regions();

        auto *phys = get_physpagemap();
        uint64_t release_lim = phys->max();
        release_lim = release_lim << 12;
        uint64_t phys_mem_watermark = phys->max() << 12;
        uint64_t end_phys_addr = phys->max() << 12;

        {
            uint64_t phys_mem_added = 0;
            uint64_t phys_mem_watermark_next = phys_mem_watermark;
            bool first_pass = true;
            do {
                if (!first_pass) {
                    get_klogger() << "Adding more physical memory:\n";
                }
                phys_mem_added = 0;

                for (const auto &bank : memory_banks) {
                    uint64_t base_addr = bank.base;
                    uint64_t length = bank.size;

                    if (first_pass) {
                        get_klogger() << " - Region " << base_addr << " (" << length << ")\n";
                    }

                    uint64_t region_end = base_addr + length;
                    if (region_end > end_phys_addr) {
                        end_phys_addr = region_end;
                    }

                    if (region_end > phys_mem_watermark) {
                        uint64_t start = base_addr >= phys_mem_watermark ? base_addr : phys_mem_watermark;
                        uint64_t last = start + 1;
                        get_klogger() << "   - Added " << start << " - ";
                        for (uint64_t addr = start; addr < (base_addr + length); addr += 0x1000) {
                            if (phys->max() <= (addr >> 12)) {
                                auto i = phys->max();
                                phys->set_max((addr >> 12) + 1);
                                while (i < phys->max()) {
                                    phys->claim(i);
                                    ++i;
                                }
                            }
                            if (addr >= release_lim && (addr >> 12) < phys->max()) {
                                phys->release(addr >> 12);
                            }
                            last = addr + 0x1000;
                            if (last > phys_mem_watermark_next) {
                                phys_mem_watermark_next = last;
                            }
                            phys_mem_added += 0x1000;
                        }
                        --last;
                        get_klogger() << last << "\n";
                    }
                }

                if (first_pass) {
                    for (const auto &res : reserved_regions) {
                        uint64_t base_addr = res.base;
                        uint64_t length = res.size;
                        if (length == 0) {
                            continue;
                        }
                        uint64_t end = base_addr + length;
                        auto end_page = (end + 0xFFF) >> 12;
                        auto start_page = base_addr >> 12;
                        auto prev_max = phys->max();
                        if (prev_max < end_page) {
                            phys->claim(prev_max, end_page - prev_max);
                        }
                        phys->claim(start_page, end_page - start_page);
                        reserved_mem.push_back(std::make_tuple<uint64_t,uint64_t>(base_addr, length));
                    }
                }

                if (phys_mem_watermark != phys_mem_watermark_next || phys_mem_added != 0) {
                    phys_mem_watermark = phys_mem_watermark_next;
                    get_klogger() << "Added " << phys_mem_added << " bytes, watermark "
                                  << phys_mem_watermark
                                  << ", end " << end_phys_addr << "\n";
                }
                first_pass = false;
            } while (phys_mem_added > 0);
            get_klogger() << "Done with mapping physical memory\n";
        }

        setup_pvpage_stats();
        setup_simplest_malloc_stats();

        for (const std::tuple<uint64_t,uint64_t> &res_mem : reserved_mem) {
            uint64_t base_addr = std::get<0>(res_mem);
            uint64_t end_addr = base_addr + std::get<1>(res_mem) - 1;
            get_klogger() << "Reserved memory range " << base_addr << " - " << end_addr << "\n";
        }

        fpu_init();
        get_klogger() << "FPU enabled\n";

        create_hw_interrupt_handler();
        create_cpu_interrupt_handler();

        install_exception_vectors();
        get_klogger() << "cpu0: Exception vectors installed\n";

    }
}

static uint8_t boot_lock_data[sizeof(*boot_lock)];

extern "C" void init_kernel() {
    bootstrap_lock();
    if (boot_lock == nullptr) {
        boot_lock = new (reinterpret_cast<void *>(boot_lock_data)) hw_spinlock();
        bootstrap_unlock();
        bootstrap_uart_puts("init_kernel preinit done\n");
    } else {
        bootstrap_unlock();
    }
    auto current_cpu = get_cpu_num();
    auto num_cpus = get_num_cpus();
    bootstrap_uart_puts("init_kernel called for core\n");
    early_init();
    if (current_cpu == 0) {
        std::lock_guard lock{*boot_lock};
        cpu_num_vector = 1;
        boot_stage_counter = 1;
    }
    if (current_cpu != 0) {
        while (true) {
            std::lock_guard lock{*boot_lock};
            if (boot_stage_counter == 1 && cpu_num_vector == current_cpu) {
                break;
            }
        }

        fpu_init();
        install_exception_vectors();
        get_klogger() << "cpu" << static_cast<uint16_t>(current_cpu) << ": Exception vectors installed\n";

        std::lock_guard lock{*boot_lock};
        cpu_num_vector = current_cpu + 1;
        if (cpu_num_vector == num_cpus) {
            boot_stage_counter = 2;
        }
    }

    if (current_cpu == 0) {
        while (true) {
            std::lock_guard lock{*boot_lock};
            if (boot_stage_counter == 2) {
                break;
            }
        }
        get_klogger() << "cpu0: All cpus have installed exception vectors\n";

        kernel_elf = new KernelElf(bootstrap_get_kernel_phys(), bootstrap_get_kernel_size());

        {
            const auto *init_table_section = kernel_elf->elf().get_elf64_header().get_init_table();
            if (init_table_section != nullptr) {
                typedef void (*init_func)();

                auto *init_table = (uintptr_t *) (void **) init_table_section->sh_addr;

                get_klogger() << "Run global construct " << ((uintptr_t) init_table) << "\n";

                auto size = init_table_section->sh_size;
                auto num = size / sizeof(*init_table);
                for (int i = 0; i < num; i++) {
                    intptr_t faddr = (intptr_t) init_table[i];
                    get_klogger() << "Construct " << ((uintptr_t) faddr) << "\n";
                    if (faddr != 0 && faddr != -1) {
                        auto func = (init_func) (void *) init_table[i];
                        func();
                    }
                }
            }

            const auto *ctor_list_section = kernel_elf->elf().get_elf64_header().get_ctors_table();
            if (ctor_list_section != nullptr) {
                typedef void (*init_func)();

                auto *ctor_list = (uintptr_t *) (void **) ctor_list_section->sh_addr;

                auto size = ctor_list_section->sh_size;
                auto num = size / sizeof(*ctor_list);

                get_klogger() << "Run global construct <.ctors>\n";
                for (int i = 0; i < num; i++) {
                    if (ctor_list[i] != 0) {
                        intptr_t faddr = (intptr_t) ctor_list[i];
                        get_klogger() << "Construct " << ((uintptr_t) faddr) << "\n";
                        ((init_func) ctor_list[i])();
                    }
                }
            }
        }
        std::cout << "Kernel ELF constructors done\n";

        scheduler = new tasklist;
        {
            scheduler->create_current_idle_task(0);
        }
        std::cout << "Scheduler created\n";
    }

    for (;;) {
        asm volatile("wfe");
    }
}