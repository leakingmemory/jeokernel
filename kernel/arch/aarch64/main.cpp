//
// Created by sigsegv on 9/17/26.
//
#include "../../serial/uart.h"
#include "smp.h"
#include "null_klogger.h"

void bootstrap_uart_puts(const char *s);
volatile void *bootstrap_get_uart();

extern "C" void init_kernel() {
    bootstrap_uart_puts("init_kernel called for core\n");
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
        get_klogger() << "cpu0: console output established\n";
    }
    for (;;) {
        asm volatile("wfe");
    }
}