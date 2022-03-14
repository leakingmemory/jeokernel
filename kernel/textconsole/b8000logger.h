//
// Created by sigsegv on 19.04.2021.
//

#ifndef JEOKERNEL_B8000LOGGER_H
#define JEOKERNEL_B8000LOGGER_H

#include "b8000.h"
#include <klogger.h>
#include <concurrency/hw_spinlock.h>

class b8000logger : public KLogger {
private:
    hw_spinlock _lock;
public:
    b8000 cons;
    b8000logger();
    ~b8000logger() override = default;
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    void erase(int backtrack, int erase) override;
    b8000logger & operator << (const char *str) override;
};


#endif //JEOKERNEL_B8000LOGGER_H
