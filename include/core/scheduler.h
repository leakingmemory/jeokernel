//
// Created by sigsegv on 08.05.2021.
//

#ifndef JEOKERNEL_SCHEDULER_H
#define JEOKERNEL_SCHEDULER_H

#include <interrupt_frame.h>
#include <klogger.h>
#include <concurrency/hw_spinlock.h>
#include <string>
#include <mutex>

#define PREALLOC_TASK_SLOTS         8192
#define PREALLOC_EVENT_LOOP         64
#define PREALLIC_SWITCH_TASK_TMP    256

/*
 * Hard grouping: Realtime always first, normal always before low and idle,
 * low always before idle, idle only when there is nothing else to do
 */
#define PRIO_GROUP_IDLE 0
#define PRIO_GROUP_LOW 1
#define PRIO_GROUP_NORMAL 2
#define PRIO_GROUP_REALTIME 3

#define MAX_POINTS 31
#define MAX_RESOURCES 7

struct task_bits {
    uint8_t priority_group : 2;
    bool running : 1;
    bool blocked : 1;
    bool end : 1;
    /*
     * Wait one cycle before releasing a task to be switched to another cpu. This will probably
     * pin tasks that are continously running to one single core, but the point is that if
     * a kernel task is preempted and immediately switched to another cpucore that'll probably
     * blow the interrupt stack still running the return.
     */
    bool stack_quarantine : 1;
    bool cpu_pinned : 1;
    uint8_t reserved1 : 1;
    uint8_t cpu;
    uint8_t points : 5;
    uint8_t resources: 3;
    uint8_t reserved2;
    uint32_t id;

    task_bits(uint8_t priority_group) : priority_group(priority_group), running(false), blocked(true), end(false), stack_quarantine(false), cpu_pinned(false), reserved1(0), cpu(0), points(0), resources(0), reserved2(0), id(0) {
    }
} __attribute__((__packed__));

class task;

class task_resource {
public:
    task_resource() {
    }
    task_resource(const task_resource &) = delete;
    task_resource(task_resource &&) = delete;
    task_resource &operator =(const task_resource &) = delete;
    task_resource &operator =(task_resource &&) = delete;

    virtual ~task_resource() {
    };

    virtual void task_enter() {
    }
    virtual void task_leave() {
    }
    virtual bool page_fault(task &current_task, Interrupt &intr) {
        return false;
    }
    virtual bool exception(task &current_task, const std::string &name, Interrupt &intr) {
        return false;
    }
};

#define TASK_EVENT_EXIT         0
#define TASK_EVENT_TIMER100HZ   1
#define TASK_EVENT_NANOTIME     2
#define TASK_EVENT_MUTEX        3
#define TASK_EVENT_CLEAR_WAIT_MUTEX 4
#define TASK_EVENT_SEMAPHORE    5
#define TASK_EVENT_CLEAR_WAIT_SEMAPHORE 6

class task_event_handler {
public:
    task_event_handler() {
    }
    virtual ~task_event_handler() {
    }

    task_event_handler(const task_event_handler &) = delete;
    task_event_handler(task_event_handler &&) = delete;
    task_event_handler & operator =(const task_event_handler &) = delete;
    task_event_handler & operator =(task_event_handler &&) = delete;

    virtual void event(uint64_t v1, uint64_t v2, uint64_t v3) {
    }
};

class task_event_handler_ref {
};

struct task_info {
    task_bits bits;
    std::string name;
};

class task_timeout100hz_handler_ref;
class tasklist;

class task {
    friend tasklist;
    friend task_timeout100hz_handler_ref;
private:
    InterruptCpuFrame cpu_frame;
    InterruptStackFrame cpu_state;
    x86_fpu_state fpu_sse_state;
    task_bits bits;
    std::vector<task_resource *> resources;
    std::vector<task_event_handler *> event_handlers;
    std::vector<std::function<void ()>> do_when_not_running;
    std::string name;
public:
    task() : cpu_frame(), cpu_state(), fpu_sse_state(), bits(PRIO_GROUP_NORMAL), resources(), event_handlers(), do_when_not_running(), name("[]") {
    }
    task(const InterruptCpuFrame &cpuFrame, const InterruptStackFrame &stackFrame, const x86_fpu_state &fpusse_state, const std::vector<task_resource *> resources)
    : cpu_frame(cpuFrame), cpu_state(stackFrame), fpu_sse_state(fpusse_state), bits(PRIO_GROUP_NORMAL), resources(resources), event_handlers(), do_when_not_running(), name("[]") {
    }

    task(const task &) = delete;

    task(task &&mv) : cpu_frame(mv.cpu_frame), cpu_state(mv.cpu_state),
                      fpu_sse_state(mv.fpu_sse_state), bits(mv.bits),
                      resources(std::move(mv.resources)), event_handlers(std::move(mv.event_handlers)),
                      do_when_not_running(std::move(mv.do_when_not_running)), name(std::move(mv.name)) {
        mv.resources.clear();
        mv.event_handlers.clear();
    }

    task &operator =(const task &) = delete;

    task &operator =(task &&) = delete;

    void set_running(bool running) {
        bits.running = running ? true : false;
        if (running) {
            for (auto *resource : resources) {
                resource->task_enter();
            }
        } else {
            for (auto *resource : resources) {
                resource->task_leave();
            }
            for (auto &func : do_when_not_running) {
                func();
            }
            do_when_not_running.clear();
        }
    }
    bool is_running() {
        return bits.running;
    }
    void when_not_running(std::function<void ()> func) {
        do_when_not_running.push_back(func);
    }
    void set_blocked(bool blocked) {
        bits.blocked = blocked ? true : false;
    }
    bool is_blocked() {
        return bits.blocked;
    }
    void set_cpu(uint8_t cpu) {
        bits.cpu = cpu;
    }
    uint8_t get_cpu() {
        return bits.cpu;
    }
    void save_state(Interrupt &interrupt) {
        cpu_frame = interrupt.get_cpu_frame();
        cpu_state = interrupt.get_cpu_state();
        fpu_sse_state = interrupt.get_fpu_state();
    }
    void restore_state(Interrupt &interrupt) {
        interrupt.set_cpu_frame(cpu_frame);
        interrupt.get_cpu_state() = cpu_state;
        interrupt.get_fpu_state() = fpu_sse_state;
    }
    void set_priority_group(uint8_t priority_group) {
        bits.priority_group = priority_group;
    }
    uint8_t get_priority_group() {
        return bits.priority_group;
    }
    void set_points(uint8_t points) {
        bits.points = points <= MAX_POINTS ? points : MAX_POINTS;
    }
    uint8_t get_points() {
        return bits.points;
    }

    void set_end(bool end) {
        bits.end = end ? true : false;
    }
    bool is_end() {
        return bits.end;
    }

    InterruptCpuFrame &get_cpu_frame() {
        return cpu_frame;
    }
    InterruptStackFrame &get_cpu_state() {
        return cpu_state;
    }

    void set_id(uint32_t id) {
        bits.id = id;
    }
    uint32_t get_id() {
        return bits.id;
    }

    void set_stack_quarantine(bool quar) {
        bits.stack_quarantine = quar ? true : false;
    }

    bool is_stack_quarantined() const {
        return bits.stack_quarantine;
    }

    void set_cpu_pinned(bool pinned) {
        bits.cpu_pinned = pinned ? true : false;
    }

    bool is_cpu_pinned() const {
        return bits.cpu_pinned;
    }

    void event(std::vector<task_event_handler *> &event_handler_loop, uint64_t v1, uint64_t v2, uint64_t v3) {
        /*
         * Because event handlers may remove themselves from the
         * list. First copy the list, then call the handlers.
         */
        if (event_handlers.empty()) {
            return;
        }
        event_handler_loop.clear();
        event_handlers.reserve(this->event_handlers.size());
        for (task_event_handler *eh : this->event_handlers) {
            event_handler_loop.push_back(eh);
        }
        for (task_event_handler *eh : event_handler_loop) {
            eh->event(v1, v2, v3);
        }
    }

    void add_event_handler(task_event_handler *eh) {
        event_handlers.push_back(eh);
    }
    bool remove_event_handler(task_event_handler *eh) {
        auto iterator = event_handlers.begin();
        while (iterator != event_handlers.end()) {
            if (*iterator == eh) {
                event_handlers.erase(iterator);
                return true;
            }
            ++iterator;
        }
        return false;
    }

    void resource_acq(int8_t res_acq) {
        res_acq += this->bits.resources;
        if (res_acq < 0) {
            res_acq = 0;
        }
        if (res_acq > MAX_RESOURCES) {
            res_acq = MAX_RESOURCES;
        }
        this->bits.resources = res_acq;
    }
    uint8_t get_resources() {
        return this->bits.resources;
    }

    task_info get_task_info() {
        task_info info{
            .bits = bits,
            .name = name
        };
        return info;
    }

    template <class T> T *get_resource() const {
        for (auto *resource : resources) {
            T *resource_with_type = dynamic_cast<T *>(resource);
            if (resource_with_type != nullptr) {
                return resource_with_type;
            }
        }
        return nullptr;
    }

    bool page_fault(Interrupt &intr) {
        for (auto *resource : resources) {
            if (resource->page_fault(*this, intr)) {
                return true;
            }
        }
        return false;
    }

    bool exception(const std::string &ename, Interrupt &intr) {
        for (auto *resource : resources) {
            if (resource->exception(*this, ename, intr)) {
                return true;
            }
        }
        return false;
    }

    void set_name(const std::string &nname) {
        this->name = nname;
    }
};

struct event_call {
    uint64_t v0;
    uint64_t v1;
    uint64_t v2;
    uint8_t res_acq;
};

class tasklist {
    friend task_timeout100hz_handler_ref;
private:
    hw_spinlock _lock;
    uint64_t tick_counter;
    uint32_t serial;
    bool multicpu;
    std::vector<task *> tasks;
    std::vector<task_event_handler *> event_handler_loop;
    std::vector<event_call> events_in_event;
    /* switch task tmps: */
    std::vector<event_call> events_in_event_tmp;
    std::vector<task *> task_pool;
    std::vector<task *> next_task_pool;
private:
    uint32_t get_next_id();
    void ticks_millisleep(uint64_t ms);
    void tsc_nanosleep(uint64_t nanos);
public:
    tasklist() : _lock(), tick_counter(0), serial(0), multicpu(false), tasks(), event_handler_loop(), events_in_event(), events_in_event_tmp(), task_pool(), next_task_pool() {
        tasks.reserve(PREALLOC_TASK_SLOTS);
        event_handler_loop.reserve(PREALLOC_EVENT_LOOP);
        task_pool.reserve(PREALLIC_SWITCH_TASK_TMP);
        next_task_pool.reserve(PREALLIC_SWITCH_TASK_TMP);
    }
    bool is_multicpu();
    uint32_t create_current_idle_task(uint8_t cpu);
    void start_multi_cpu(uint8_t cpu);
    void switch_tasks(Interrupt &interrupt, uint8_t cpu);

    bool page_fault(Interrupt &interrupt);
    bool exception(const std::string &name, Interrupt &interrupt);

    uint32_t new_task(uint64_t rip, uint16_t cs, uint64_t rdi, uint64_t rsi,
                  uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9,
                  const std::vector<task_resource *> &resources);
    uint32_t new_task(uint64_t rip, uint16_t cs, uint16_t ds, uint64_t fsbase, uint64_t rbp, uint64_t rsp,
                      uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8,
                      uint64_t r9, const std::vector<task_resource *> &resources);

    bool terminate_blocked(task *t);

    void exit(uint8_t cpu, bool returnToCallerIfNotTerminatedImmediately = false);
    void user_exit(int64_t returnValue);

    uint32_t get_current_task_id();
    void join(uint32_t task_id);

    template <class T> T *get_resource(const task &t) {
        critical_section cli{};
        std::lock_guard lock{_lock};
        return t.template get_resource<T>();
    }
    void when_not_running(task &t, std::function<void ()> func);

    task &get_task_with_lock(uint32_t task_id);
    task *get_nullable_task_with_lock(uint32_t task_id);
    task &get_current_task_with_lock();
    task &get_current_task();

    void all_tasks(std::function<void (task &)> func);

    void add_task_event_handler(task_event_handler *handler);
    /**
     * Consider using critical_section to disable interrupts while
     * if the blocked state is not completely assured yet.
     *
     * @param blocked
     */
    void set_blocked(bool blocked, int8_t resource_acq = 0);

    void event_in_event_handler(uint64_t v0, uint64_t v1, uint64_t v2, uint8_t res_acq = 0);

    void event(uint64_t v0, uint64_t v1, uint64_t v2, int8_t res_acq = 0);
    void event_with_lock(uint64_t v0, uint64_t v1, uint64_t v2, int8_t res_acq);

    void millisleep(uint64_t ms);

    void sleep(uint64_t seconds) {
        millisleep(seconds * 1000);
    }
    void usleep(uint64_t us);
    void nanosleep(uint64_t us);

    std::shared_ptr<task_event_handler_ref> set_timeout_millis(uint64_t ms, const std::function<void ()> &callback);

    /**
     * Returns true when tasks are now runnable for _current_ cpu. If
     * true call int 0xFE to switch tasks.
     *
     * @param cpu
     * @return
     */
    bool clear_stack_quarantines(uint8_t cpu);

    std::vector<task_info> get_task_infos();

    bool set_name(uint32_t pid, const std::string &name);
    void set_name(const std::string &name);
};

tasklist *get_scheduler();
uint64_t get_ticks();

#endif //JEOKERNEL_SCHEDULER_H
