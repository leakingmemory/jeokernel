//
// Created by sigsegv on 25.04.2021.
//

#ifndef JEOKERNEL_STACK_H
#define JEOKERNEL_STACK_H

#include <stdint.h>

class normal_stack {
private:
    uint64_t addr;
public:
    normal_stack();
    ~normal_stack();

    /*
     * No copying
     */
    normal_stack(const normal_stack &) = delete;
    normal_stack & operator = (const normal_stack &) = delete;

    uint64_t get_addr() const {
        return addr;
    }
};

#endif //JEOKERNEL_STACK_H
