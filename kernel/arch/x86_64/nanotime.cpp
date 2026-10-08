//
// Created by sigsegv on 11.05.2021.
//

#include <core/nanotime.h>

uint64_t tsc_offset = 0;
uint64_t tsc_frequency = 0;
uint64_t stored_ref = 0;
uint64_t stored_tsc = 0;

bool is_nanotime_available() {
    return tsc_offset > 0 && tsc_frequency > 0;
}

bool is_nanotime_reliable() {
    return is_nanotime_available();
}

uint64_t read_tsc() {
    uint64_t high;
    uint64_t low;
    asm("rdtsc; mov %%rdx, %0; mov %%rax, %1" : "=r"(high), "=r"(low) :: "%rax", "%rdx");
    low += high << 32;
    return low;
}

uint64_t get_nanotime_ref() {
    uint64_t tsc = read_tsc();
    uint64_t frequency = tsc_frequency / 10;
    if (tsc < tsc_offset || frequency == 0) {
        return 0;
    }
    tsc -= tsc_offset;
    uint64_t time_high = ((tsc >> 32) * 100000000);
    uint64_t time_low = ((tsc & 0xFFFFFFFF) * 100000000);
    time_low += (time_high % frequency) << 32;
    time_high /= frequency;
    time_low /= frequency;
    return (time_high << 32) + time_low;
}

void set_nanotime_ref(uint64_t ref) {
    uint64_t tsc = read_tsc();
    if (stored_tsc > 0 && stored_ref < ref && stored_tsc < tsc) {
        uint64_t tsc_diff = tsc - stored_tsc;
        uint64_t ref_diff = ref - stored_ref;
        tsc_diff *= 1000000000; // targetting Hz accuracy
        tsc_frequency = tsc_diff / ref_diff;
        tsc_offset = tsc - ((ref * tsc_frequency) / 1000000000);
    }
    stored_ref = ref;
    stored_tsc = tsc;
}

uint64_t get_tsc_frequency() {
    return tsc_frequency;
}

uint64_t get_tsc_offset() {
    return tsc_offset;
}
