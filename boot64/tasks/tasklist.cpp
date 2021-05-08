//
// Created by sigsegv on 08.05.2021.
//

#include <concurrency/critical_section.h>
#include <mutex>
#include <klogger.h>
#include <core/scheduler.h>

void tasklist::create_current_idle_task(uint8_t cpu) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    task &t = tasks.emplace_back();
    t.set_running(true);
    t.set_blocked(false);
    t.set_cpu(cpu);
    t.set_priority_group(PRIO_GROUP_IDLE);
}

void tasklist::switch_tasks(Interrupt &interrupt, uint8_t cpu) {
    std::vector<task *> task_pool{};
    std::vector<task *> next_task_pool{};
    task *current_task;

    task_pool.reserve(16);
    next_task_pool.reserve(16);

    critical_section cli{};
    std::lock_guard lock{_lock};

    for (auto &t : tasks) {
        if (t.get_cpu() == cpu && t.is_running()) {
            current_task = &t;
        }
        if (t.get_priority_group() == PRIO_GROUP_REALTIME && !t.is_blocked() && ((&t) == current_task || !t.is_running())) {
            task_pool.push_back(&t);
        } else if (t.get_priority_group() == PRIO_GROUP_NORMAL && !t.is_blocked() && ((&t) == current_task || !t.is_running())){
            next_task_pool.push_back(&t);
        }
    }
    if (task_pool.empty()) {
        task_pool = next_task_pool;
    }
    if (task_pool.empty()) {
        for (auto &t : tasks) {
            if (t.get_priority_group() == PRIO_GROUP_LOW && !t.is_blocked() && ((&t) == current_task || !t.is_running())) {
                task_pool.push_back(&t);
            } else if (t.get_priority_group() == PRIO_GROUP_IDLE && !t.is_blocked() && ((&t) == current_task || !t.is_running())){
                next_task_pool.push_back(&t);
            }
        }
    }
    if (task_pool.empty()) {
        task_pool = next_task_pool;
    }
    if (task_pool.empty()) {
        wild_panic("Task lists are empty");
    }
    bool in_current_pool = false;
    for (auto *t : task_pool) {
        if (t == current_task) {
            in_current_pool = true;
            break;
        }
    }
    auto points_on_license = current_task->get_points();
    if (points_on_license == MAX_POINTS) {
        if (in_current_pool) {
            for (auto *t : task_pool) {
                auto points = t->get_points();
                if (points > 0) {
                    t->set_points(points - 1);
                }
            }
        }
    } else {
        current_task->set_points(points_on_license + 1);
    }
    task *win = nullptr;
    for (task *t : task_pool) {
        if (win == nullptr || (t->get_points() < win->get_points())) {
            win = t;
        }
    }
    if (win != current_task) {
        current_task->set_running(false);
        current_task->save_state(interrupt);
        win->restore_state(interrupt);
        win->set_running(true);
        win->set_cpu(cpu);
    }
}
