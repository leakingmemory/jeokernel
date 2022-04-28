//
// Created by sigsegv on 19.04.2021.
//

#include <klogger.h>

static KLogger *klogger = nullptr;

void set_klogger(KLogger *n_klogger) {
    klogger = n_klogger;
}

KLogger &get_klogger() {
    return *klogger;
}

extern "C" {
void wild_panic(const char *str) {
    auto &log = get_klogger();
    log << "Panic in the wild: " << str << "\n";
    while (1) {
        asm("hlt;");
    }
}
}