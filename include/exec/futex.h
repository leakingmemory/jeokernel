//
// Created by sigsegv on 8/17/23.
//

#ifndef JEOKERNEL_FUTEX_H
#define JEOKERNEL_FUTEX_H

#include <sys/types.h>
#include <concurrency/hw_spinlock.h>
#include <vector>
#include <functional>

#define FUTEX_WAIT  0
#define FUTEX_WAKE  1

#define FUTEX_PRIVATE           0x080
#define FUTEX_CLOCK_REALTIME    0x100

struct FutexWaiting {
    std::function<void ()> wakeup;
    uintptr_t addr;
    pid_t pid;
    pid_t tid;

    FutexWaiting(const std::function<void ()> &wakeup, uintptr_t addr, pid_t pid, pid_t tid) : wakeup(wakeup), addr(addr), pid(pid), tid(tid) {}
};

class Process;
class callctx;

class Futex {
private:
    hw_spinlock mtx{};
    std::vector<FutexWaiting> waiting{};
public:
    static Futex &Instance();
    int WakeAll(Process &process, uintptr_t uptr_addr);
    int Wake(Process &process, uintptr_t uptr_addr, int wakeNum);
    int Wait(std::shared_ptr<callctx> ctx, uintptr_t uptr_addr, uint32_t val);
};

#endif //JEOKERNEL_FUTEX_H
