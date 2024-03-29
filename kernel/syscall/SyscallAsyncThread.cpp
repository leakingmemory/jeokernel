//
// Created by sigsegv on 9/9/22.
//

#include "SyscallAsyncThread.h"
#include <exec/WorkingForThread.h>
#include <thread>

SyscallAsyncThread::~SyscallAsyncThread() {
    std::thread *t;
    {
        std::lock_guard lock{mtx};
        stop = true;
        t = thr;
    }
    if (t != nullptr) {
        t->join();
        delete t;
    }
}

void SyscallAsyncThread::Queue(uint32_t task_id, const std::function<void()> &f) {
    std::lock_guard lock{mtx};
    std::function<void()> func{f};
    queue.emplace_back([func, task_id] () mutable {
        WorkingForThread workingForThread{task_id};
        func();
    });
    sema.release();
    if (thr == nullptr && !stop) {
        thr = new std::thread([this] () {
            std::this_thread::set_name(name);
            while (true) {
                sema.acquire();
                std::vector<std::function<void()>> q{};
                {
                    std::lock_guard lock{mtx};
                    if (stop) {
                        break;
                    }
                    for (const auto &qi : queue) {
                        q.push_back(qi);
                    }
                    queue.clear();
                }
                for (auto qi : q) {
                    qi();
                }
            }
        });
    }
}
