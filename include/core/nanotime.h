//
// Created by sigsegv on 11.05.2021.
//

#ifndef JEOKERNEL_NANOTIME_H
#define JEOKERNEL_NANOTIME_H

#include <cstdint>

/**
 * Nanoseconds time source is available?
 * @return
 */
bool is_nanotime_available();

/**
 * Nanoseconds time source is reliable?
 * @return
 */
bool is_nanotime_reliable();

/**
 * Nanoseconds since boot.
 *
 * @return
 */
uint64_t get_nanotime_ref();

/**
 * Call twice with correct nanotime to adjust frequency.
 *
 * @param ref
 */
void set_nanotime_ref(uint64_t ref);

uint64_t get_tsc_frequency();

uint64_t get_tsc_offset();

#endif //JEOKERNEL_NANOTIME_H
