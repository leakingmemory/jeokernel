//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_WORKINGFORTHREAD_H
#define JEOKERNEL_WORKINGFORTHREAD_H

#include "core/scheduler.h"

class WorkingForThread {
private:
    tasklist *scheduler;
    uint32_t task_id;
public:
    WorkingForThread(uint32_t task_id);
    ~WorkingForThread();
};


#endif //JEOKERNEL_WORKINGFORTHREAD_H
