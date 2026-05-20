//
// Created by sigsegv on 19.04.2021.
//

#include <klogger.h>

#include "mutex"
#include "concurrency/hw_spinlock.h"

struct KLoggerRecord {
    KLoggerRecord *next;
    KLogger *klogger;
    int handle;
};

static KLogger *klogger = nullptr;
static hw_spinlock *klrec_lock = nullptr;
static KLoggerRecord *klrec = nullptr;
static int handle_c = 0;

constexpr size_t max_num_consoles = 16;

class CommonKLogger : public KLogger {
public:
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    void erase(int backtrack, int erase) override;
    KLogger & operator << (const char *str) override;
};

uint32_t CommonKLogger::GetWidth() {
    KLogger *primary;
    {
        std::lock_guard lock{*klrec_lock};
        primary = klrec != nullptr ? klrec->klogger : nullptr;
    }
    if (primary != nullptr) {
        return primary->GetWidth();
    } else {
        return 0;
    }
}
uint32_t CommonKLogger::GetHeight() {
    KLogger *primary;
    {
        std::lock_guard lock{*klrec_lock};
        primary = klrec != nullptr ? klrec->klogger : nullptr;
    }
    if (primary != nullptr) {
        return primary->GetHeight();
    } else {
        return 0;
    }
}
void CommonKLogger::print_at(uint8_t col, uint8_t row, const char *str) {
    KLogger *primary;
    {
        std::lock_guard lock{*klrec_lock};
        primary = klrec != nullptr ? klrec->klogger : nullptr;
    }
    if (primary != nullptr) {
        primary->print_at(col, row, str);
    }
};
void CommonKLogger::erase(int backtrack, int erase) {
    KLogger *consoles[max_num_consoles];
    size_t num;
    {
        std::lock_guard lock{*klrec_lock};
        auto *rec = klrec;
        num = 0;
        while (rec != nullptr && num < max_num_consoles) {
            consoles[num] = rec->klogger;
            rec = rec->next;
            ++num;
        }
    }
    for (size_t i = 0; i < num; i++) {
        consoles[i]->erase(backtrack, erase);
    }
}

KLogger & CommonKLogger::operator << (const char *str) {
    KLogger *consoles[max_num_consoles];
    size_t num;
    {
        std::lock_guard lock{*klrec_lock};
        auto *rec = klrec;
        num = 0;
        while (rec != nullptr && num < max_num_consoles) {
            consoles[num] = rec->klogger;
            rec = rec->next;
            ++num;
        }
    }
    for (size_t i = 0; i < num; i++) {
        (*consoles[i]) << str;
    }
    return *this;
};

int add_klogger(KLogger *n_klogger) {
    if (klrec_lock == nullptr) {
        klrec_lock = new hw_spinlock();
    }
    bool need_klogger_install;
    {
        std::lock_guard lock{*klrec_lock};
        need_klogger_install = klogger == nullptr;
    }
    CommonKLogger *n_c_kl = need_klogger_install ? new CommonKLogger() : nullptr;
    auto *klrec_n = new KLoggerRecord();
    klrec_n->klogger = n_klogger;
    {
        std::lock_guard lock{*klrec_lock};
        klrec_n->next = klrec;
        klrec_n->handle = handle_c++;
        klrec = klrec_n;
        if (n_c_kl != nullptr && klogger == nullptr) {
            klogger = n_c_kl;
            n_c_kl = nullptr;
        }
    }
    if (n_c_kl != nullptr) {
        delete n_c_kl;
    }
    return klrec_n->handle;
}

void replace_klogger(int h, KLogger *klogger) {
    std::lock_guard lock{*klrec_lock};
    auto *klr = klrec;
    while (klr != nullptr) {
        if (klr->handle == h) {
            klr->klogger = klogger;
            return;
        }
        klr = klr->next;
    }
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