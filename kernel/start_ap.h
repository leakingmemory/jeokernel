//
// Created by sigsegv on 05.05.2021.
//

#ifndef JEOKERNEL_START_AP_H
#define JEOKERNEL_START_AP_H

#include <cstdint>
#include <concurrency/raw_spinlock.h>

extern "C" {
void ap_trampoline();
void ap_trampoline_end();
};

raw_spinlock *get_ap_start_lock();
/**
 *
 * @return Pointer to AP start counter
 */
class GlobalDescriptorTable;
const uint32_t *install_ap_bootstrap(GlobalDescriptorTable &);

#endif //JEOKERNEL_START_AP_H
