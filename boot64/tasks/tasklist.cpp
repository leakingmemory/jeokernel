//
// Created by sigsegv on 08.05.2021.
//

#include <concurrency/critical_section.h>
#include <mutex>
#include <klogger.h>
#include <core/scheduler.h>
#include <stack.h>

class task_stack_resource : public task_resource {
private:
    normal_stack stack;
public:
    task_stack_resource() : task_resource(), stack() {
    }

    ~task_stack_resource() override {
    }

    uint64_t get_address() {
        return stack.get_addr();
    }
};

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
        if (t.get_priority_group() == PRIO_GROUP_REALTIME && !t.is_blocked() && !t.is_end() && ((&t) == current_task || !t.is_running())) {
            task_pool.push_back(&t);
        } else if (t.get_priority_group() == PRIO_GROUP_NORMAL && !t.is_blocked() && !t.is_end() && ((&t) == current_task || !t.is_running())){
            next_task_pool.push_back(&t);
        }
    }
    if (task_pool.empty()) {
        task_pool = next_task_pool;
    }
    if (task_pool.empty()) {
        for (auto &t : tasks) {
            if (t.get_priority_group() == PRIO_GROUP_LOW && !t.is_blocked() && !t.is_end() && ((&t) == current_task || !t.is_running())) {
                task_pool.push_back(&t);
            } else if (t.get_priority_group() == PRIO_GROUP_IDLE && !t.is_blocked() && !t.is_end() && ((&t) == current_task || !t.is_running())){
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
        if (!current_task->is_end()) {
            current_task->save_state(interrupt);
        }
        win->restore_state(interrupt);
        win->set_running(true);
        win->set_cpu(cpu);
        if (current_task->is_end()) {
            auto iterator = tasks.begin();
            while (iterator != tasks.end()) {
                task &t = *iterator;
                if (&t == current_task) {
                    tasks.erase(iterator);
                }
                ++iterator;
                goto evicted_task;
            }
            wild_panic("Unable to find current<ending> task in list");
evicted_task:
            ;
        }
    } else if (current_task == nullptr) {
        wild_panic("No task is current");
    } else if (current_task->is_end()) {
        wild_panic("Current task has requested stop, but can't be");
    }
}

void tasklist::new_task(uint64_t rip, uint16_t cs, uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8,
                        uint64_t r9, const std::vector<task_resource *> &resources) {
    std::vector<task_resource *> my_resources{};
    for (auto resource : resources) {
        my_resources.push_back(resource);
    }
    x86_fpu_state fpusse_state{};
    InterruptStackFrame cpu_state{
        .ds = 0x10,
        .es = 0x10,
        .fs = 0x10,
        .gs = 0x10,
        .rdi = rdi,
        .rsi = rsi,
        .rcx = rcx,
        .r8 = r8,
        .r9 = r9
    };
    InterruptCpuFrame cpu_frame{
        .ss = 0x10,
        .rip = rip,
        .cs = cs,
        .rflags = 0x202
    };
    task_stack_resource *stack_resource = new task_stack_resource;
    cpu_state.rbp = stack_resource->get_address();
    cpu_frame.rsp = cpu_state.rbp;

    my_resources.push_back(stack_resource);

    critical_section cli{};
    std::lock_guard lock{_lock};

    task &t = tasks.emplace_back(cpu_frame, cpu_state, fpusse_state, my_resources);
    t.set_blocked(false);
}

void tasklist::exit(uint8_t cpu) {
    task *current_task = nullptr;

    {
        critical_section cli{};
        std::lock_guard lock{_lock};

        for (auto &t : tasks) {
            if (t.get_cpu() == cpu && t.is_running()) {
                current_task = &t;
                break;
            }
        }

        if (current_task == nullptr) {
            wild_panic("current task was not found");
        }

        current_task->set_end(true);
    }

    /*
     * Wait for being evicted
     */
    while (true) {
        asm("hlt");
    }
}
