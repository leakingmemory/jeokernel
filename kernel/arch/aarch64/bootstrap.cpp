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

extern "C" [[noreturn]] void _start(Stage1Data *stage1Data) {
    stage1Data->early_init_lock.lock();
    uart_spinlock = &(stage1Data->early_init_lock);
    stage1Data->early_init_lock.unlock();

    uart = reinterpret_cast<volatile unsigned int *>(stage1Data->uart);
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    uint64_t cpu_id = mpidr & 0xFF;
    print_cpu_entry(uart, cpu_id, stage1Data->cpu_count);

    if (stage1Data->early_init_lock.try_lock()) {
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
        extend_to_advanced_physpagemap();

        bootstrap_uart_puts("Early init ends\n");

        stage1Data->early_init_lock.unlock();
    }

    for (;;) {
        asm volatile("wfe");
    }
}
