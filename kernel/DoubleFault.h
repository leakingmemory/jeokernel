//
// Created by sigsegv on 12/30/21.
//

#ifndef JEOKERNEL_DOUBLEFAULT_H
#define JEOKERNEL_DOUBLEFAULT_H


#include <thread>

class DoubleFault {
private:
    /* Careful with lifecycle, invalid outside exception stack frame */
    Interrupt &interrupt;
public:
    DoubleFault(Interrupt &interrupt) : interrupt(interrupt) {
    }
    DoubleFault(DoubleFault &&) = delete;
    DoubleFault(DoubleFault &) = delete;
    DoubleFault &operator = (DoubleFault &&) = delete;
    DoubleFault &operator = (DoubleFault &) = delete;

    void handle();
};


#endif //JEOKERNEL_DOUBLEFAULT_H
