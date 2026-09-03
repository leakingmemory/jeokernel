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
#include <concurrency/hw_spinlock.h>
#include <pagealloc.h>

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
    hw_spinlock uart_spinlock{};

    void uart_putc(volatile unsigned int *uart, char c) {
        volatile unsigned int *const uartfr = uart + (0x18 / sizeof(unsigned int));
        constexpr unsigned int UARTFR_TXFF = 1u << 5;
        while (*uartfr & UARTFR_TXFF) {
            // spin until FIFO has space
        }
        *uart = static_cast<unsigned int>(c);
    }

    void uart_puts(volatile unsigned int *uart, const char *s) {
        for (; *s != '\0'; ++s) {
            if (*s == '\n') {
                uart_putc(uart, '\r');
            }
            uart_putc(uart, *s);
        }
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

    void print_cpu_entry(volatile unsigned int *uart, uint64_t cpu_id, uint64_t cpu_count) {
        uart_spinlock.lock();
        uart_puts(uart, "AArch64 kernel entrypoint reached on CPU ");
        uart_put_dec(uart, cpu_id);
        uart_puts(uart, " / ");
        uart_put_dec(uart, cpu_count);
        uart_puts(uart, " with paging enabled!\n");
        uart_spinlock.unlock();
    }
}

extern "C" [[noreturn]] void _start(Stage1Data *stage1Data) {
    if (stage1Data->cpu_id == 0) {
        set_pagetable_virt_offset(stage1Data->phys_mem_base);

        /*
         * Let's try to alloc a stack
         */
        set_init_pml4t(stage1Data->root_pt);
    }
    volatile unsigned int *uart = reinterpret_cast<volatile unsigned int *>(stage1Data->uart);
    print_cpu_entry(uart, stage1Data->cpu_id, stage1Data->cpu_count);

    for (;;) {
        asm volatile("wfe");
    }
}
