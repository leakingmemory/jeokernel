//
// Created by sigsegv on 25.04.2021.
//

#ifndef JEOKERNEL_INTERRUPTTASKSTATE_H
#define JEOKERNEL_INTERRUPTTASKSTATE_H

#include "TaskStateSegment.h"
#include <stack.h>

class InterruptTaskState {
    normal_stack supervisor_stack;
    normal_stack ist1;
    normal_stack ist2;
    normal_stack ist3;
    normal_stack ist4;
    normal_stack ist5;
    normal_stack ist6;
    normal_stack ist7;
    TSS_entry &tsse;
public:
    InterruptTaskState(TaskStateSegment &tss);
    ~InterruptTaskState();

    InterruptTaskState(const InterruptTaskState &) = delete;
    InterruptTaskState &operator =(const InterruptTaskState &) = delete;
};


#endif //JEOKERNEL_INTERRUPTTASKSTATE_H
