//
// Created by sigsegv on 8/13/23.
//

#include <exec/blockthread.h>
#include <exec/WorkingForThread.h>

blockthread::blockthread() : thr([this] () {
    while (true) {
        bool run;
        std::function<void ()> func{};
        {
            std::lock_guard lock{spinlock};
            if (stop) {
                return;
            }
            auto iterator = queue.begin();
            if (iterator != queue.end()) {
                func = *iterator;
                run = true;
                queue.erase(iterator);
            }
        }
        if (run) {
            func();
        }
        sema.acquire();
    }
}){
}

blockthread::~blockthread() {
    Stop();
}

void blockthread::Queue(uint32_t task_id, const std::function<void()> &i_func) {
    std::function<void ()> func{i_func};
    std::lock_guard lock{spinlock};
    queue.push_back([func, task_id] () mutable {
        WorkingForThread w{task_id};
        func();
    });
    sema.release();
}

void blockthread::Stop() {
    {
        std::lock_guard lock{spinlock};
        stop = true;
        sema.release();
    }
    thr.join();
}
