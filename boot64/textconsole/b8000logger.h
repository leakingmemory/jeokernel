//
// Created by sigsegv on 19.04.2021.
//

#ifndef JEOKERNEL_B8000LOGGER_H
#define JEOKERNEL_B8000LOGGER_H

#include "b8000.h"
#include <klogger.h>

class b8000logger : public KLogger {
public:
    b8000 cons;
    b8000logger();
    ~b8000logger();
    b8000logger & operator << (const char *str) override;
};


#endif //JEOKERNEL_B8000LOGGER_H
