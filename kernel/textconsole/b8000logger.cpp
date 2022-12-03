//
// Created by sigsegv on 19.04.2021.
//

#include <mutex>
#include <concurrency/critical_section.h>
#include "b8000logger.h"

b8000logger::b8000logger() : KLogger(), cons() {
}

b8000logger & b8000logger::operator << (const char *str) {
    std::lock_guard lock(_lock);
    cons << str;
    return *this;
}

void b8000logger::print_at(uint8_t col, uint8_t row, const char *str) {
    std::lock_guard lock(_lock);
    cons.print_at(col, row, str);
}

void b8000logger::erase(int backtrack, int erase) {
    std::lock_guard lock(_lock);
    cons.erase(backtrack, erase);
}

uint32_t b8000logger::GetWidth() {
    return 80;
}

uint32_t b8000logger::GetHeight() {
    return 25;
}
