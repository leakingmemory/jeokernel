//
// Created by sigsegv on 9/8/22.
//

#ifndef JEOKERNEL_ACCESS_H
#define JEOKERNEL_ACCESS_H

#include <concurrency/hw_spinlock.h>
#include <concurrency/raw_semaphore.h>
#include <functional>
#include <vector>
#include <thread>
#include "SyscallHandler.h"

class ProcThread;

class Access : public Syscall {
    hw_spinlock mtx;
    raw_semaphore sema;
    std::vector<std::function<void ()>> queue;
    std::thread *thr;
    bool stop;
public:
    Access(SyscallHandler &handler) : Syscall(handler, 21), mtx(), sema(-1), queue(), thr(nullptr), stop(false) {}
    ~Access();
    Access(const SyscallHandler &) = delete;
    Access(SyscallHandler &&) = delete;
    Access &operator =(const Access &) = delete;
    Access &operator =(Access &&) = delete;
    void Queue(const std::function<void ()> &f);
    int DoAccess(ProcThread &proc, std::string filename, int mode);
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_ACCESS_H
