//
// Created by sigsegv on 8/13/23.
//

#include <exec/blockthread.h>

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

void blockthread::Queue(const std::function<void()> &func) {
    std::lock_guard lock{spinlock};
    queue.push_back(func);
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
