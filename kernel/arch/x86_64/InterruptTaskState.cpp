//
// Created by sigsegv on 25.04.2021.
//

#include "InterruptTaskState.h"

InterruptTaskState::InterruptTaskState(TaskStateSegment &tss)
: supervisor_stack(), ist1(), ist2(), ist3(), ist4(), ist5(), ist6(), ist7(), tsse(tss.get_entry())
{
    tsse.rsp0 = supervisor_stack.get_addr();
    tsse.rsp1 = supervisor_stack.get_addr();
    tsse.rsp2 = supervisor_stack.get_addr();
    tsse.ist1 = ist1.get_addr();
    tsse.ist2 = ist2.get_addr();
    tsse.ist3 = ist3.get_addr();
    tsse.ist4 = ist4.get_addr();
    tsse.ist5 = ist5.get_addr();
    tsse.ist6 = ist6.get_addr();
    tsse.ist7 = ist7.get_addr();
}

InterruptTaskState::~InterruptTaskState() {
    tsse.rsp0 = 0;
    tsse.rsp1 = 0;
    tsse.rsp2 = 0;
    tsse.ist1 = 0;
    tsse.ist2 = 0;
    tsse.ist3 = 0;
    tsse.ist4 = 0;
    tsse.ist5 = 0;
    tsse.ist6 = 0;
    tsse.ist7 = 0;
}
