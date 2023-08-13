//
// Created by sigsegv on 8/13/23.
//

#include "exec/WorkingForThread.h"

WorkingForThread::WorkingForThread(uint32_t task_id) :
        scheduler(get_scheduler()),
        task_id(scheduler->get_working_for_task_id()) {
    scheduler->set_working_for_task_id(task_id);
}
WorkingForThread::~WorkingForThread() {
    scheduler->set_working_for_task_id(task_id);
}
