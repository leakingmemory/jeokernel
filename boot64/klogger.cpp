//
// Created by sigsegv on 19.04.2021.
//

#include <klogger.h>

static KLogger *klogger = nullptr;

void set_klogger(KLogger *n_klogger) {
    klogger = n_klogger;
    *n_klogger << "Received\n";
}

KLogger &get_klogger() {
    return *klogger;
}

void wild_panic(const char *str) {
    auto &log = get_klogger();
    log << "Panic in the wild: " << str << "\n";
    while (1) {
        asm("hlt;");
    }
}