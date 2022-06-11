//
// Created by sigsegv on 30.04.2021.
//

#ifndef JEOKERNEL_PAGEFAULT_H
#define JEOKERNEL_PAGEFAULT_H

#include <interrupt_frame.h>

class PageFault {
private:
    /* Careful with lifecycle, invalid outside exception stack frame */
    Interrupt &interrupt;
public:
    PageFault(Interrupt &interrupt) : interrupt(interrupt) {
    }
    PageFault(PageFault &&) = delete;
    PageFault(PageFault &) = delete;
    PageFault &operator = (PageFault &&) = delete;
    PageFault &operator = (PageFault &) = delete;

    bool handle();
};


#endif //JEOKERNEL_PAGEFAULT_H
