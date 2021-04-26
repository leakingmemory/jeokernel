//
// Created by sigsegv on 26.04.2021.
//

#include <klogger.h>

extern "C" {
    void interrupt_handler(uint64_t interrupt_vector, void *stack_frame) {
        get_klogger() << "\nReceived interrupt " << interrupt_vector << "\n";
        wild_panic("unknown interrupt");
    }
}