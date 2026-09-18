//
// Created by sigsegv on 9/17/26.
//

void bootstrap_uart_puts(const char *s);

extern "C" void init_kernel() {
    bootstrap_uart_puts("init_kernel called for core\n");
    for (;;) {
        asm volatile("wfe");
    }
}