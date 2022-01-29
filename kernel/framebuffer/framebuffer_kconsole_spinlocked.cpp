//
// Created by sigsegv on 12/26/21.
//

#include <concurrency/critical_section.h>
#include <mutex>
#include "framebuffer_kconsole_spinlocked.h"

void framebuffer_kconsole_spinlocked::print_at(uint8_t col, uint8_t row, const char *str) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    targetObject->print_at(col, row, str);
}

framebuffer_kconsole_spinlocked &framebuffer_kconsole_spinlocked::operator<<(const char *str) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    (*targetObject) << str;
    return *this;
}

void framebuffer_kconsole_spinlocked::erase(int backtrack, int erase) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    targetObject->erase(backtrack, erase);
}
