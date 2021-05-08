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
    uint64_t reserved : 48;

    task_bits(uint8_t priority_group) : priority_group(priority_group), running(false), blocked(true), points(0), cpu(0), reserved(0) {
    }
} __attribute__((__packed__));

class task {
private:
    InterruptCpuFrame cpu_frame;
    InterruptStackFrame cpu_state;
    x86_fpu_state fpu_sse_state;
    task_bits bits;
public:
    task() : cpu_frame(), cpu_state(), fpu_sse_state(), bits(PRIO_GROUP_NORMAL) {
    }

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
};

class tasklist {
private:
    hw_spinlock _lock;
    std::vector<task> tasks;
public:
    tasklist() : _lock(), tasks() {
    }
    void create_current_idle_task(uint8_t cpu);
    void switch_tasks(Interrupt &interrupt, uint8_t cpu);
};

tasklist *get_scheduler();

#endif //JEOKERNEL_SCHEDULER_H
