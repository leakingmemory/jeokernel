//
// Created by sigsegv on 9/23/26.
//

#ifndef JEOKERNEL_AARCH64_BOOTSTRAP_H
#define JEOKERNEL_AARCH64_BOOTSTRAP_H

#include <stdint.h>

void bootstrap_uart_put_hex(uint64_t v);
void bootstrap_uart_puts(const char *s);
volatile void *bootstrap_get_uart();
uint64_t bootstrap_get_dtb();

#endif //JEOKERNEL_AARCH64_BOOTSTRAP_H
