//
// Created by sigsegv on 08.05.2021.
//

#ifndef JEOKERNEL_SCHEDULER_H
#define JEOKERNEL_SCHEDULER_H

#include <interrupt_frame.h>

/*
 * Hard grouping: Realtime always first, normal always before low and idle,
 * low always before idle, idle only when there is nothing else to do
 */
#define PRIO_GROUP_IDLE 0
#define PRIO_GROUP_LOW 1
#define PRIO_GROUP_NORMAL 2
#define PRIO_GROUP_REALTIME 3

#define MAX_POINTS 7

struct task_bits {
    uint8_t priority_group : 2;
    bool running : 1;
    bool blocked : 1;
    bool end : 1;
    uint8_t points : 3;
    uint8_t cpu;
    /*
     * Wait one cycle before releasing a task to be switched to another cpu. This will probably
     * pin tasks that are continously running to one single core, but the point is that if
     * a kernel task is preempted and immediately switched to another cpucore that'll probably
     * blow the interrupt stack still running the return.
     */
    bool stack_quarantine : 1;
    uint16_t reserved : 15;
    uint32_t id;

    task_bits(uint8_t priority_group) : priority_group(priority_group), running(false), blocked(true), end(false), points(0), cpu(0), stack_quarantine(false), reserved(0), id(0) {
    }
} __attribute__((__packed__));

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
};

#define TASK_EVENT_EXIT     0

class task_event_handler {
public:
    task_event_handler() {
    }
    virtual ~task_event_handler() {
    }

    virtual void event(uint64_t v1, uint64_t v2, uint64_t v3) {
    }
};

class task {
private:
    InterruptCpuFrame cpu_frame;
    InterruptStackFrame cpu_state;
    x86_fpu_state fpu_sse_state;
    task_bits bits;
    std::vector<task_resource *> resources;
    std::vector<task_event_handler *> event_handlers;
public:
    task() : cpu_frame(), cpu_state(), fpu_sse_state(), bits(PRIO_GROUP_NORMAL), resources(), event_handlers() {
    }
    task(const InterruptCpuFrame &cpuFrame, const InterruptStackFrame &stackFrame, const x86_fpu_state &fpusse_state, const std::vector<task_resource *> resources)
    : cpu_frame(cpuFrame), cpu_state(stackFrame), fpu_sse_state(fpusse_state), bits(PRIO_GROUP_NORMAL), resources(resources), event_handlers() {
    }

    task(const task &) = delete;

    task(task &&mv) : cpu_frame(mv.cpu_frame), cpu_state(mv.cpu_state),
                      fpu_sse_state(mv.fpu_sse_state), bits(mv.bits),
                      resources(std::move(mv.resources)), event_handlers(std::move(mv.event_handlers)) {
        mv.resources.clear();
        mv.event_handlers.clear();
    }

    task &operator =(const task &) = delete;

    void set_running(bool running) {
        bits.running = running ? true : false;
    }
    bool is_running() {
        return bits.running;
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

    void set_id(uint32_t id) {
        bits.id = id;
    }
    uint32_t get_id() {
        return bits.id;
    }

    void set_stack_quarantine(bool quar) {
        bits.stack_quarantine = quar ? true : false;
    }

    bool is_stack_quarantined() {
        return bits.stack_quarantine;
    }

    void event(uint64_t v1, uint64_t v2, uint64_t v3) {
        /*
         * Because event handlers may remove themselves from the
         * list. First copy the list, then call the handlers.
         */
        if (event_handlers.empty()) {
            return;
        }
        std::vector<task_event_handler *> event_handlers{};
        event_handlers.reserve(this->event_handlers.size());
        for (task_event_handler *eh : this->event_handlers) {
            event_handlers.push_back(eh);
        }
        for (task_event_handler *eh : event_handlers) {
            eh->event(v1, v2, v3);
        }
    }

    void add_event_handler(task_event_handler *eh) {
        event_handlers.push_back(eh);
    }
    void remove_event_handler(task_event_handler *eh) {
        auto iterator = event_handlers.begin();
        while (iterator != event_handlers.end()) {
            if (*iterator == eh) {
                event_handlers.erase(iterator);
                return;
            }
        }
    }
};

class tasklist {
private:
    hw_spinlock _lock;
    uint32_t serial;
    std::vector<task *> tasks;
private:
    uint32_t get_next_id();
public:
    tasklist() : _lock(), tasks() {
    }
    void create_current_idle_task(uint8_t cpu);
    void switch_tasks(Interrupt &interrupt, uint8_t cpu);

    uint32_t new_task(uint64_t rip, uint16_t cs, uint64_t rdi, uint64_t rsi,
                  uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9,
                  const std::vector<task_resource *> &resources);

    void exit(uint8_t cpu);

    uint32_t get_current_task_id();
    void join(uint32_t task_id);

    task &get_task_with_lock(uint32_t task_id);
    task &get_current_task_with_lock();
};

tasklist *get_scheduler();

#endif //JEOKERNEL_SCHEDULER_H
