//
// Created by sigsegv on 9/3/21.
//

#include <delay.h>
#include <core/nanotime.h>
#include <concurrency/critical_section.h>

void delay_nano(uint64_t nanos) {
    critical_section cli{};
    nanos += get_nanotime_ref();
    while (nanos > get_nanotime_ref()) {
        asm("pause");
    }
}