//
// Created by sigsegv on 9/22/26.
//

#ifndef JEOKERNEL_NULL_KLOGGER_H
#define JEOKERNEL_NULL_KLOGGER_H

#include "klogger.h"

class null_klogger : public KLogger {
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    void erase(int backtrack, int erase) override;
};


#endif //JEOKERNEL_NULL_KLOGGER_H
