//
// Created by sigsegv on 9/22/26.
//

#include "null_klogger.h"

uint32_t null_klogger::GetWidth() {
    return 80;
}
uint32_t null_klogger::GetHeight() {
    return 25;
}
void null_klogger::print_at(uint8_t, uint8_t, const char *) {
}
void null_klogger::erase(int, int) {
}
