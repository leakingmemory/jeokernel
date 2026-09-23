//
// Minimal aarch64 kernel entry.
//
// The real aarch64 kernel does not exist yet. For now this file's only job is
// to give the aarch64 build a kernel image to produce, which the arm64 shim
// embeds (see arm64shim/wrapbin.S) and will later hand control to. _start just
// parks the core.
//

#include <stage1.h>
#include <pagetable.h>
#include <pagealloc.h>
#include <physpagemap.h>
#include <vpallocator.h>

#include "stack.h"
#include "bootstrap.h"

extern "C" int atexit(void (*)(void)) {
    return 0;
}

extern "C" int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}

uintptr_t pagetable_virt_offset = 0;

uintptr_t get_pagetable_virt_offset() {
    return pagetable_virt_offset;
}

void set_pagetable_virt_offset(uintptr_t offset) {
    pagetable_virt_offset = offset;
}

namespace {
    raw_spinlock *uart_spinlock;
    volatile unsigned int *uart;
    uint64_t dtb_ptr{0};

    void uart_puts(volatile unsigned int *uart, const char *s);

    void uart_putc(volatile unsigned int *uart, char c) {
        volatile unsigned int *const uartfr = uart + (0x18 / sizeof(unsigned int));
        constexpr unsigned int UARTFR_TXFF = 1u << 5;
        while (*uartfr & UARTFR_TXFF) {
            // spin until FIFO has space
        }
        *uart = static_cast<unsigned int>(c);
    }

    void uart_put_dec(volatile unsigned int *uart, uint64_t v) {
        if (v == 0) {
            uart_putc(uart, '0');
            return;
        }
        char buf[32];
        int idx = 0;
        while (v > 0) {
            buf[idx++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
        for (int i = idx - 1; i >= 0; --i) {
            uart_putc(uart, buf[i]);
        }
    }

    void uart_put_hex(volatile unsigned int *uart, uint64_t v) {
        uart_putc(uart, '0');
        uart_putc(uart, 'x');
        if (v == 0) {
            uart_putc(uart, '0');
            return;
        }
        char buf[32];
        int idx = 0;
        while (v > 0) {
            auto d = v % 16;
            buf[idx++] = static_cast<char>(d < 10 ? ('0' + d) : ('A' + d - 10));
            v /= 16;
        }
        for (int i = idx - 1; i >= 0; --i) {
            uart_putc(uart, buf[i]);
        }
    }

    void uart_puts(volatile unsigned int *uart, const char *s) {
        for (; *s != '\0'; ++s) {
            if (*s == '\n') {
                uart_putc(uart, '\r');
            }
            uart_putc(uart, *s);
        }
    }
}

void bootstrap_uart_put_hex(uint64_t v) {
    uart_put_hex(uart, v);
}

void bootstrap_uart_puts(const char *s) {
    uart_puts(uart, s);
}

volatile void *bootstrap_get_uart() {
    return uart;
}

uint64_t bootstrap_get_dtb() {
    return dtb_ptr;
}

namespace {
    void print_cpu_entry(volatile unsigned int *uart, uint64_t cpu_id, uint64_t cpu_count) {
        uart_spinlock->lock();
        bootstrap_uart_puts("AArch64 kernel entrypoint reached on CPU ");
        uart_put_dec(uart, cpu_id);
        bootstrap_uart_puts(" / ");
        uart_put_dec(uart, cpu_count);
        bootstrap_uart_puts(" with paging enabled!\n");
        uart_spinlock->unlock();
    }
}

extern "C" void init_kernel();

static uint32_t *cpuids;
static uint32_t num_cpus_entered{0};

int get_cpu_num() {
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    uint32_t cpu_id = mpidr & 0xFFFFFF;
    for (uint32_t i = 0; i < num_cpus_entered; i++) {
        if (cpuids[i] == cpu_id) {
            return i;
        }
    }
    return -1;
}

extern "C" [[noreturn]] void _start(Stage1Data *stage1Data) {
    stage1Data->early_init_lock.lock();
    uart_spinlock = &(stage1Data->early_init_lock);
    stage1Data->early_init_lock.unlock();

    uart = reinterpret_cast<volatile unsigned int *>(stage1Data->uart);
    dtb_ptr = stage1Data->dtb;
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    uint32_t cpu_id = mpidr & 0xFFFFFF;
    print_cpu_entry(uart, cpu_id, stage1Data->cpu_count);

    if (stage1Data->early_init_lock.try_lock()) {
        bool run_init{false};
        {
            stage1Data->smp_synch_lock.lock();
            if (stage1Data->boot_stage_counter == 0) {
                stage1Data->boot_stage_counter = 1;
                run_init = true;
            }
            stage1Data->smp_synch_lock.unlock();
        }
        if (run_init) {
            bootstrap_uart_puts("Early init starts\n");

            set_pagetable_virt_offset(stage1Data->phys_mem_base);

            VPAllocatorPage *vpalloc_root = reinterpret_cast<VPAllocatorPage *>(stage1Data->vpalloc_root_vaddr);
            {
                auto *vpalloc_page = vpalloc_root;
                while (vpalloc_page) {
                    bootstrap_uart_puts("vpalloc_page import");
                    auto physaddr = vpalloc_page->next.paddr.addr;
                    if (physaddr != 0) {
                        auto vaddr = physaddr + stage1Data->phys_mem_base;
                        auto vptr = reinterpret_cast<VPAllocatorPage *>(vaddr);
                        vpalloc_page->SetNextVirtual(vptr);
                        vpalloc_page = vpalloc_page->GetNext();
                    } else {
                        bootstrap_uart_puts(": last page\n");
                        vpalloc_page = nullptr;
                    }
                }
            }
            set_vpalloc_root(vpalloc_root);

            /*
             * Let's try to alloc a stack
             */
            init_mapping_pages(stage1Data->phys_mem_base + stage1Data->mem_mapper_8pages);
            set_init_pml4t(stage1Data->root_pt);
            init_simple_physpagemap(stage1Data->ppmap + stage1Data->phys_mem_base, stage1Data->ppmap_base_page);
            initialize_pagetable_control();
            setup_simplest_malloc_impl();

            while (stage1Data->HasFreePhys()) {
                auto free_phys = stage1Data->GetFreePhys();
                bootstrap_uart_puts("Free phys: page = ");
                bootstrap_uart_put_hex(static_cast<uint64_t>(free_phys.page));
                bootstrap_uart_puts(", num = ");
                bootstrap_uart_put_hex(static_cast<uint64_t>(free_phys.num));
                bootstrap_uart_puts("\n");
                ppagefree(static_cast<uint64_t>(free_phys.page) << 12, static_cast<uint64_t>(free_phys.num) << 12);
            }

            extend_to_advanced_physpagemap(stage1Data->ppmap, stage1Data->ppmap_base_page);

            cpuids = reinterpret_cast<decltype(cpuids)>(malloc(sizeof(*cpuids) * stage1Data->cpu_count));

            bootstrap_uart_puts("Early init ends\n");

            stage1Data->early_init_lock.unlock();

            stage1Data->smp_synch_lock.lock();
            stage1Data->boot_stage_counter = 2;
            stage1Data->smp_synch_lock.unlock();
        }
    }

    bootstrap_uart_puts("Waiting for cpu counter\n");
    while (true) {
        stage1Data->smp_synch_lock.lock();
        if (stage1Data->boot_stage_counter > 1) {
            cpuids[num_cpus_entered] = cpu_id;
            ++num_cpus_entered;
            if (num_cpus_entered == stage1Data->cpu_count) {
                stage1Data->boot_stage_counter = 3;
            }
            stage1Data->smp_synch_lock.unlock();
            break;
        }
        stage1Data->smp_synch_lock.unlock();
    }
    stage1Data->early_init_lock.lock();
    bootstrap_uart_puts("Waiting for all cores\n");
    stage1Data->early_init_lock.unlock();
    while (true) {
        stage1Data->smp_synch_lock.lock();
        if (stage1Data->boot_stage_counter >= 2) {
            stage1Data->smp_synch_lock.unlock();
            break;
        }
        stage1Data->smp_synch_lock.unlock();
    }
    stage1Data->early_init_lock.lock();
    bootstrap_uart_puts("Waiting for bootstrap\n");
    stage1Data->early_init_lock.unlock();
    while (true) {
        stage1Data->smp_synch_lock.lock();
        if (stage1Data->boot_stage_counter == 3) {
            stage1Data->smp_synch_lock.unlock();
            break;
        }
        stage1Data->smp_synch_lock.unlock();
    }

    stage1Data->early_init_lock.lock();
    auto *stage1_stack = new normal_stack;
    uint64_t stack = stage1_stack->get_addr();
    uint64_t init_kernel_addr = reinterpret_cast<uint64_t>(reinterpret_cast<void *>(init_kernel));
    bootstrap_uart_puts("Calling init sp=");
    bootstrap_uart_put_hex(stack);
    bootstrap_uart_puts(" pc=");
    bootstrap_uart_put_hex(init_kernel_addr);
    bootstrap_uart_puts("\n");
    stage1Data->early_init_lock.unlock();
    asm("mov x0, %0; mov sp, x0; mov x1, %1; br x1" ::"r"(stack), "r"(init_kernel_addr));

    bootstrap_uart_puts("_start: should not reach\n");

    for (;;) {
        asm volatile("wfe");
    }
}
