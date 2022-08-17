//
// Created by sigsegv on 08.05.2021.
//

#include <concurrency/critical_section.h>
#include <mutex>
#include <klogger.h>
#include <core/scheduler.h>
#include <stack.h>
#include <sstream>
#include <core/nanotime.h>
#include <pagealloc.h>
#include "../ApStartup.h"

#define CONTINUE_RUNNING_BIAS 12
#define RESOURCE_PRIORITY_BIAS 12

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

bool tasklist::is_multicpu() {
    critical_section cli{};
    std::lock_guard lock{_lock};
    return multicpu;
}

uint32_t tasklist::create_current_idle_task(uint8_t cpu) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    task *t = new task();
    uint32_t pid = get_next_id();
    t->set_id(pid);
    t->set_running(nullptr, cpu);
    t->set_blocked(false);
    t->set_cpu(cpu);
    t->set_priority_group(PRIO_GROUP_IDLE);
    t->set_cpu_pinned(true);
    tasks.push_back(t);
    return pid;
}

void tasklist::start_multi_cpu(uint8_t cpu) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    for (task *t : tasks) {
        t->set_cpu(cpu);
    }
    multicpu = true;
}

void tasklist::switch_tasks(Interrupt &interrupt, uint8_t cpu) {
    task *current_task;

    critical_section cli{};
    std::lock_guard lock{_lock};

    task_pool.clear();
    next_task_pool.clear();

    bool nanos_avail = is_nanotime_available();
    uint64_t nanos{nanos_avail ? get_nanotime_ref() : 0};

    for (task *t : tasks) {
        if (nanos_avail) {
            t->event(event_handler_loop, TASK_EVENT_NANOTIME, nanos, cpu);
        }
        if (!multicpu || t->get_cpu() == cpu) {
            if (t->is_stack_quarantined()) {
                t->set_stack_quarantine(false);
            } else if (t->is_running()) {
                current_task = t;
            }
        } else if (t->is_cpu_pinned()) {
            continue;
        }

        if (t->get_priority_group() == PRIO_GROUP_REALTIME && !t->is_blocked() && !t->is_end() && !t->is_stack_quarantined() && (t == current_task || !t->is_running())) {
            task_pool.push_back(t);
        } else if (t->get_priority_group() == PRIO_GROUP_NORMAL && !t->is_blocked() && !t->is_end() && !t->is_stack_quarantined() && (t == current_task || !t->is_running())){
            next_task_pool.push_back(t);
        }
    }
    if (task_pool.empty()) {
        task_pool = next_task_pool;
    }
    if (task_pool.empty()) {
        for (task *t : tasks) {
            if (multicpu && t->is_cpu_pinned() && t->get_cpu() != cpu) {
                continue;
            }
            if (t->get_priority_group() == PRIO_GROUP_LOW && !t->is_blocked() && !t->is_end() && !t->is_stack_quarantined() && (t == current_task || !t->is_running())) {
                task_pool.push_back(t);
            } else if (t->get_priority_group() == PRIO_GROUP_IDLE && !t->is_blocked() && !t->is_end() && !t->is_stack_quarantined() && (t == current_task || !t->is_running())){
                next_task_pool.push_back(t);
            }
        }
    }
    if (task_pool.empty()) {
        task_pool = next_task_pool;
    }
    if (task_pool.empty()) {
        std::stringstream str{};
        str << std::dec << "For cpu " << (unsigned int) cpu << " no viable tasks to pick\n" << std::hex;
        for (task *t : tasks) {
            str << std::hex << (uint64_t) (t) << ": " << (t->is_running() ? "R" : "S") << (t->is_blocked() ? "B" : "A") << std::dec << " cpu"<< (unsigned int) t->get_cpu() << ((t == current_task) ? "*" : "") << "\n";
        }
        get_klogger() << str.str().c_str();
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
        auto points = t->get_points();
        if (t == current_task) {
            if (points > CONTINUE_RUNNING_BIAS) {
                points -= CONTINUE_RUNNING_BIAS;
            } else {
                points = 0;
            }
        }
        if (t->get_resources() > 0) {
            if (points > RESOURCE_PRIORITY_BIAS) {
                points -= RESOURCE_PRIORITY_BIAS;
            } else {
                points = 0;
            }
        }
        if (win == nullptr || (points < win->get_points())) {
            win = t;
        }
    }
    if (win != current_task) {
        if (!current_task->is_end()) {
            current_task->set_stack_quarantine(true);
            current_task->save_state(interrupt);
        }

        current_task->set_not_running();

        win->restore_state(interrupt);
        win->set_running(&interrupt, multicpu ? cpu : 0);
        if (multicpu) {
            win->set_cpu(cpu);
        }
        if (current_task->is_end()) {
            auto iterator = tasks.begin();
            while (iterator != tasks.end()) {
                task *t = *iterator;
                if (t == current_task) {
                    tasks.erase(iterator);
                    delete t;
                    goto evicted_task;
                }
                ++iterator;
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

bool tasklist::page_fault(Interrupt &interrupt) {
    critical_section cli{};
    uint8_t cpu = 0;
    if (multicpu) {
        ApStartup *apStartup = GetApStartup();
        cpu = apStartup->GetCpuNum();
    }
    task *current_task = nullptr;

    {
        std::lock_guard lock{_lock};

        for (task *t : tasks) {
            if (t->get_cpu() == cpu && t->is_running()) {
                current_task = t;
                break;
            }
        }

        if (current_task == nullptr) {
            wild_panic("current task was not found");
        }
    }

    bool handled = current_task->page_fault(interrupt);
    switch_tasks(interrupt, cpu);
    return handled;
}

bool tasklist::exception(const std::string &name, Interrupt &interrupt) {
    critical_section cli{};
    uint8_t cpu = 0;
    if (multicpu) {
        ApStartup *apStartup = GetApStartup();
        cpu = apStartup->GetCpuNum();
    }
    task *current_task = nullptr;

    {
        std::lock_guard lock{_lock};

        for (task *t : tasks) {
            if (t->get_cpu() == cpu && t->is_running()) {
                current_task = t;
                break;
            }
        }

        if (current_task == nullptr) {
            wild_panic("current task was not found");
        }
    }

    bool handled = current_task->exception(name, interrupt);
    switch_tasks(interrupt, cpu);
    return handled;
}

uint32_t tasklist::new_task(uint64_t rip, uint16_t cs, uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8,
                            uint64_t r9, const std::vector<task_resource *> &resources) {
    std::vector<task_resource *> my_resources{};
    for (auto resource : resources) {
        my_resources.push_back(resource);
    }

    task_stack_resource *stack_resource = new task_stack_resource;
    uint64_t rbp = stack_resource->get_address();
    uint64_t rsp = rbp;

    my_resources.push_back(stack_resource);

    return new_task(rip, cs, 0x10, 0, rbp, rsp, rdi, rsi, rdx, rcx, r8, r9, my_resources);
}


uint32_t tasklist::new_task(uint64_t rip, uint16_t cs, uint16_t ds, uint64_t fsbase, uint64_t rbp, uint64_t rsp, uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8,
                        uint64_t r9, const std::vector<task_resource *> &resources) {
    x86_fpu_state fpusse_state{};
    InterruptStackFrame cpu_state{
        .ds = ds,
        .es = ds,
        .fs = ds,
        .gs = ds,
        .fsbase = fsbase,
        .r9 = r9,
        .r8 = r8,
        .rsi = rsi,
        .rdi = rdi,
        .rbp = rbp,
        .rdx = rdx,
        .rcx = rcx
    };
    InterruptCpuFrame cpu_frame{
        .rip = rip,
        .cs = cs,
        .rflags = 0x202,
        .rsp = rsp,
        .ss = ds
    };

    critical_section cli{};
    std::lock_guard lock{_lock};

    task *t = new task(cpu_frame, cpu_state, fpusse_state, resources);
    tasks.push_back(t);
    t->set_id(get_next_id());
    t->set_blocked(false);
    return t->get_id();
}

bool tasklist::terminate_blocked(task *end_task) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    auto iterator = tasks.begin();
    while (iterator != tasks.end()) {
        task *t = *iterator;
        if (t == end_task) {
            tasks.erase(iterator);
            delete t;
            return true;
        }
        ++iterator;
    }
    return false;
}

void tasklist::exit(uint8_t cpu, bool returnToCallerIfNotTerminatedImmediately) {
    task *current_task = nullptr;

    {
        critical_section cli{};
        std::lock_guard lock{_lock};

        if (!multicpu) {
            cpu = 0;
        }

        for (task *t : tasks) {
            if (t->get_cpu() == cpu && t->is_running()) {
                current_task = t;
                break;
            }
        }

        if (current_task == nullptr) {
            wild_panic("current task was not found");
        }

        current_task->set_end(true);

        for (task *t : tasks) {
            if (t != current_task) {
                t->event(event_handler_loop, TASK_EVENT_EXIT, current_task->get_id(), 0);
            }
        }
    }

    asm("int $0xFE"); // Request task switch now.

    if (!returnToCallerIfNotTerminatedImmediately) {
        /*
         * Wait for being evicted
         */
        while (true) {
            asm("hlt");
        }
    }
}

void tasklist::user_exit(int64_t returnValue) {
    task *current_task = nullptr;

    int cpu;
    if (multicpu) {
        cpu = GetApStartup()->GetCpuNum();
    } else {
        cpu = 0;
    }

    critical_section cli{};
    std::lock_guard lock{_lock};

    for (task *t : tasks) {
        if (t->get_cpu() == cpu && t->is_running()) {
            current_task = t;
            break;
        }
    }

    if (current_task == nullptr) {
        wild_panic("current task was not found");
    }

    current_task->set_end(true);

    for (task *t : tasks) {
        if (t != current_task) {
            t->event(event_handler_loop, TASK_EVENT_EXIT, current_task->get_id(), 0);
        }
    }
}

uint32_t tasklist::get_next_id() {
    while (true) {
        uint32_t id = ++serial;
        bool found = false;
        for (task *t : tasks) {
            if (id == t->get_id()) {
                found = true;
                break;
            }
        }
        if (!found) {
            return id;
        }
    }
}

class task_join_handler : public task_event_handler {
private:
    uint32_t current_task_id;
    uint32_t other_task_id;
public:
    task_join_handler(uint32_t current_task_id, uint32_t other_task_id) :
    task_event_handler(),
    current_task_id(current_task_id), other_task_id(other_task_id) {
    }

    ~task_join_handler() {
    }

    void event(uint64_t event_id, uint64_t pid, uint64_t ignored) override {
        if (event_id == TASK_EVENT_EXIT && pid == other_task_id) {
            task &t = get_scheduler()->get_task_with_lock(current_task_id);
            t.set_blocked(false);
            t.remove_event_handler(this);
            delete this;
        }
    }
};

void tasklist::join(uint32_t task_id) {
    {
        critical_section cli{};
        std::lock_guard lock{_lock};

        task &t = get_current_task_with_lock();
        task *joining = get_nullable_task_with_lock(task_id);
        if (joining == nullptr || joining->is_end()) {
            return;
        }
        t.add_event_handler(new task_join_handler(t.get_id(), task_id));
        t.set_blocked(true);
    }

    asm("int $0xFE"); // Request task switch now.

    while (true) {
        {
            critical_section cli{};
            std::lock_guard lock{_lock};
            task &t = get_current_task_with_lock();
            if (!t.is_blocked()) {
                return;
            }
        }
        for (int i = 0; i < 10; i++) {
            asm("pause");
        }
    }
}

void tasklist::when_not_running(task &t, std::function<void()> func) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    if (!t.is_running()) {
        func();
        return;
    }
    t.when_not_running(func);
}

uint32_t tasklist::get_current_task_id() {
    critical_section cli{};

    uint8_t cpu_num = 0;

    task *current_task;

    std::lock_guard lock{_lock};

    if (multicpu) {
        ApStartup *apStartup = GetApStartup();
        cpu_num = apStartup->GetCpuNum();
    }

    for (task *t : tasks) {
        if (t->get_cpu() == cpu_num && t->is_running()) {
            current_task = t;
            break;
        }
    }

    if (current_task == nullptr) {
        wild_panic("No current task found");
    }

    return current_task->get_id();
}

task &tasklist::get_task_with_lock(uint32_t task_id) {
    for (task *t : tasks) {
        if (t->get_id() == task_id) {
            return *t;
        }
    }
    wild_panic("Task not found");
    while (true) {
        asm("hlt");
    }
}

task *tasklist::get_nullable_task_with_lock(uint32_t task_id) {
    for (task *t : tasks) {
        if (t->get_id() == task_id) {
            return t;
        }
    }
    return nullptr;
}

task &tasklist::get_current_task_with_lock() {
    uint8_t cpu_num = 0;
    if (multicpu) {
        ApStartup *apStartup = GetApStartup();
        cpu_num = apStartup->GetCpuNum();
    }

    task *current_task;

    for (task *t : tasks) {
        if (t->get_cpu() == cpu_num && t->is_running()) {
            current_task = t;
        }
    }

    if (current_task == nullptr) {
        wild_panic("No current task found");
    }

    return *current_task;
}

task &tasklist::get_current_task() {
    critical_section cli{};
    std::lock_guard lock{_lock};
    return get_current_task_with_lock();
}

void tasklist::all_tasks(std::function<void(task &)> func) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    for (auto *t : tasks) {
        func(*t);
    }
}

void tasklist::event_in_event_handler(uint64_t v0, uint64_t v1, uint64_t v2, uint8_t res_acq) {
    event_call ev{
        .v0 = v0,
        .v1 = v1,
        .v2 = v2,
        .res_acq = res_acq
    };
    events_in_event.push_back(ev);
}

void tasklist::event(uint64_t v0, uint64_t v1, uint64_t v2, int8_t res_acq) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    event_with_lock(v0, v1, v2, res_acq);
    while (!events_in_event.empty()) {
        events_in_event_tmp.clear();
        for (const auto &ev : events_in_event) {
            events_in_event_tmp.push_back(ev);
        }
        events_in_event.clear();
        for (const auto &ev: events_in_event_tmp) {
            event_with_lock(ev.v0, ev.v1, ev.v2, ev.res_acq);
        }
    }
}
void tasklist::event_with_lock(uint64_t v0, uint64_t v1, uint64_t v2, int8_t res_acq) {
    reload_pagetables();

    if (v0 == TASK_EVENT_TIMER100HZ) {
        tick_counter = v1;
    }
    for (task *t : tasks) {
        t->event(event_handler_loop, v0, v1, v2);
    }
    if (res_acq != 0) {
        get_current_task_with_lock().resource_acq(res_acq);
    }
}

class task_nanos_handler : public task_event_handler {
private:
    uint64_t wakeup_time;
    uint32_t current_task_id;
public:
    task_nanos_handler(uint32_t current_task_id, uint64_t wakeup_time) :
            task_event_handler(), current_task_id(current_task_id), wakeup_time(wakeup_time) {
    }

    ~task_nanos_handler() {
    }

    void event(uint64_t event_id, uint64_t nanos, uint64_t cpu) override {
        if (event_id == TASK_EVENT_NANOTIME && nanos >= wakeup_time) {
            task &t = get_scheduler()->get_task_with_lock(current_task_id);
            t.set_blocked(false);
            t.remove_event_handler(this);
            delete this;
        }
    }
};

class task_timer100hz_handler : public task_event_handler {
private:
    uint64_t wakeup_time;
    uint32_t current_task_id;
public:
    task_timer100hz_handler(uint32_t current_task_id, uint64_t wakeup_time) :
    task_event_handler(), current_task_id(current_task_id), wakeup_time(wakeup_time) {
    }

    ~task_timer100hz_handler() {
    }

    void event(uint64_t event_id, uint64_t currect_tick, uint64_t ignored) override {
        if (event_id == TASK_EVENT_TIMER100HZ && currect_tick >= wakeup_time) {
            task &t = get_scheduler()->get_task_with_lock(current_task_id);
            t.set_blocked(false);
            t.remove_event_handler(this);
            delete this;
        }
    }
};

class task_timeout100hz_handler : public task_event_handler {
private:
    uint64_t wakeup_time;
    uint32_t current_task_id;
    std::function<void ()> callback;
public:
    task_timeout100hz_handler(uint32_t current_task_id, uint64_t wakeup_time, const std::function<void ()> &callback) :
    task_event_handler(), current_task_id(current_task_id), wakeup_time(wakeup_time), callback(callback) {
    }

    ~task_timeout100hz_handler() {
    }

    void event(uint64_t event_id, uint64_t currect_tick, uint64_t ignored) override {
        if (event_id == TASK_EVENT_TIMER100HZ && currect_tick >= wakeup_time) {
            task &t = get_scheduler()->get_task_with_lock(current_task_id);
            t.remove_event_handler(this);
            callback();
            delete this;
        }
    }
};

class task_timeout100hz_handler_ref : public task_event_handler_ref {
private:
    tasklist *scheduler;
    task *the_task;
    task_timeout100hz_handler *handler;
public:
    task_timeout100hz_handler_ref(tasklist *scheduler, task *the_task, task_timeout100hz_handler *handler);
    ~task_timeout100hz_handler_ref();
};

void tasklist::ticks_millisleep(uint64_t ms) {
    uint64_t ticks100hz = (ms / 10) + 1;
    {
        critical_section cli{};
        std::lock_guard lock{_lock};

        task &t = get_current_task_with_lock();
        t.add_event_handler(new task_timer100hz_handler(t.get_id(), tick_counter + ticks100hz));
        t.set_blocked(true);
    }

    asm("int $0xFE"); // Request task switch now.

    while (true) {
        {
            critical_section cli{};
            std::lock_guard lock{_lock};
            task &t = get_current_task_with_lock();
            if (!t.is_blocked()) {
                return;
            }
        }
        for (int i = 0; i < 1000; i++) {
            asm("pause");
        }
    }
}

void tasklist::tsc_nanosleep(uint64_t nanos) {
    nanos += get_nanotime_ref() + 1;
    {
        critical_section cli{};
        std::lock_guard lock{_lock};

        task &t = get_current_task_with_lock();
        t.add_event_handler(new task_nanos_handler(t.get_id(), nanos));
        t.set_blocked(true);
    }

    asm("int $0xFE"); // Request task switch now.

    while (true) {
        {
            critical_section cli{};
            std::lock_guard lock{_lock};
            task &t = get_current_task_with_lock();
            if (!t.is_blocked()) {
                return;
            }
        }
        for (int i = 0; i < 1000; i++) {
            asm("pause");
        }
    }
}

void tasklist::millisleep(uint64_t ms) {
    if (ms > 1000000000 || !is_nanotime_reliable()) {
        ticks_millisleep(ms);
    } else {
        tsc_nanosleep(ms * 1000000);
    }
}

void tasklist::usleep(uint64_t us) {
    if (us > 1000000000000 || !is_nanotime_reliable()) {
        if (us % 1000)
        ticks_millisleep((us / 1000) + ((us % 1000) > 0 ? 1 : 0));
    } else {
        tsc_nanosleep(us * 1000);
    }
}

void tasklist::nanosleep(uint64_t nanos) {
    if (is_nanotime_reliable()) {
        tsc_nanosleep(nanos);
    } else {
        millisleep((nanos / 1000000) + ((nanos % 1000000) > 0 ? 1 : 0));
    }
}

void tasklist::add_task_event_handler(task_event_handler *handler) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    task &t = get_current_task_with_lock();
    t.add_event_handler(handler);
}

void tasklist::set_blocked(bool blocked, int8_t res_acq) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    task &t = get_current_task_with_lock();
    t.set_blocked(blocked);

}

bool tasklist::clear_stack_quarantines(uint8_t cpu) {
    bool runnable{false};

    critical_section cli{};
    std::lock_guard lock{_lock};

    for (task *t : tasks) {
        if (t->get_cpu() == cpu && t->is_stack_quarantined()) {
            t->set_stack_quarantine(false);
        }
        if (!t->is_running() && !t->is_blocked() && !t->is_end() && (!t->is_stack_quarantined() || t->get_cpu() == cpu)) {
            runnable = true;
        }
    }

    return runnable;
}

std::vector<task_info> tasklist::get_task_infos() {
    std::vector<task_info> infos{};
    {
        critical_section cli{};
        std::lock_guard lock{_lock};
        for (task *t : tasks) {
            infos.push_back(t->get_task_info());
        }
    }
    return infos;
}

bool tasklist::set_name(uint32_t pid, const std::string &name) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    for (task *t : tasks) {
        if (t->get_id() == pid) {
            t->set_name(name);
            return true;
        }
    }
    return false;
}

void tasklist::set_name(const std::string &name) {
    critical_section cli{};
    std::lock_guard lock{_lock};
    task &t = get_current_task_with_lock();
    t.set_name(name);
}

std::shared_ptr<task_event_handler_ref> tasklist::set_timeout_millis(uint64_t ms, const std::function<void ()> &callback) {
    uint64_t ticks100hz = (ms / 10) + 1;
    critical_section cli{};
    std::lock_guard lock{_lock};
    task &t = get_current_task_with_lock();
    auto *handler = new task_timeout100hz_handler(t.get_id(), ticks100hz + tick_counter, callback);
    t.add_event_handler(handler);
    std::shared_ptr<task_timeout100hz_handler_ref> ref{new task_timeout100hz_handler_ref(this, &t, handler)};
    return ref;
}

task_timeout100hz_handler_ref::task_timeout100hz_handler_ref(tasklist *scheduler, task *the_task, task_timeout100hz_handler *handler) : scheduler(scheduler), the_task(the_task), handler(handler) {}

task_timeout100hz_handler_ref::~task_timeout100hz_handler_ref() {
    critical_section cli{};
    std::lock_guard lock{scheduler->_lock};
    auto iterator = the_task->event_handlers.begin();
    while (iterator != the_task->event_handlers.end()) {
        if (*iterator == handler) {
            the_task->event_handlers.erase(iterator);
            delete handler;
            break;
        }
        ++iterator;
    }
}